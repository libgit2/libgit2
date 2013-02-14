/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include <zlib.h>

#include "git2/indexer.h"
#include "git2/object.h"
#include "git2/oid.h"

#include "common.h"
#include "pack.h"
#include "mwindow.h"
#include "posix.h"
#include "pack.h"
#include "filebuf.h"

#define UINT31_MAX (0x7FFFFFFF)

struct entry {
	git_oid oid;
	uint32_t crc;
	uint32_t offset;
	uint64_t offset_long;
};

struct git_indexer {
	struct git_pack_file *pack;
	size_t nr_objects;
	git_vector objects;
	git_filebuf file;
	unsigned int fanout[256];
	git_oid hash;
};

struct git_indexer_stream {
	unsigned int parsed_header :1,
		opened_pack :1,
		have_stream :1,
		have_delta :1;
	struct git_pack_file *pack;
	git_filebuf pack_file;
	git_off_t off;
	git_off_t entry_start;
	git_packfile_stream stream;
	size_t nr_objects;
	git_vector objects;
	git_vector deltas;
	unsigned int fanout[256];
	git_hash_ctx hash_ctx;
	git_oid hash;
	git_transfer_progress_callback progress_cb;
	void *progress_payload;
	char objbuf[8*1024];
};

struct delta_info {
	git_off_t delta_off;
};

const git_oid *git_indexer_hash(const git_indexer *idx)
{
	return &idx->hash;
}

const git_oid *git_indexer_stream_hash(const git_indexer_stream *idx)
{
	return &idx->hash;
}

static int open_pack(struct git_pack_file **out, const char *filename)
{
	size_t namelen;
	struct git_pack_file *pack;
	struct stat st;
	int fd;

	namelen = strlen(filename);
	pack = git__calloc(1, sizeof(struct git_pack_file) + namelen + 1);
	GITERR_CHECK_ALLOC(pack);

	memcpy(pack->pack_name, filename, namelen + 1);

	if (p_stat(filename, &st) < 0) {
		giterr_set(GITERR_OS, "Failed to stat packfile.");
		goto cleanup;
	}

	if ((fd = p_open(pack->pack_name, O_RDONLY)) < 0) {
		giterr_set(GITERR_OS, "Failed to open packfile.");
		goto cleanup;
	}

	pack->mwf.fd = fd;
	pack->mwf.size = (git_off_t)st.st_size;

	*out = pack;
	return 0;

cleanup:
	git__free(pack);
	return -1;
}

static int parse_header(struct git_pack_header *hdr, struct git_pack_file *pack)
{
	int error;

	/* Verify we recognize this pack file format. */
	if ((error = p_read(pack->mwf.fd, hdr, sizeof(*hdr))) < 0) {
		giterr_set(GITERR_OS, "Failed to read in pack header");
		return error;
	}

	if (hdr->hdr_signature != ntohl(PACK_SIGNATURE)) {
		giterr_set(GITERR_INDEXER, "Wrong pack signature");
		return -1;
	}

	if (!pack_version_ok(hdr->hdr_version)) {
		giterr_set(GITERR_INDEXER, "Wrong pack version");
		return -1;
	}

	return 0;
}

static int objects_cmp(const void *a, const void *b)
{
	const struct entry *entrya = a;
	const struct entry *entryb = b;

	return git_oid_cmp(&entrya->oid, &entryb->oid);
}

static int cache_cmp(const void *a, const void *b)
{
	const struct git_pack_entry *ea = a;
	const struct git_pack_entry *eb = b;

	return git_oid_cmp(&ea->sha1, &eb->sha1);
}

int git_indexer_stream_new(
		git_indexer_stream **out,
		const char *prefix,
		git_transfer_progress_callback progress_cb,
		void *progress_payload)
{
	git_indexer_stream *idx;
	git_buf path = GIT_BUF_INIT;
	static const char suff[] = "/pack";
	int error;

	idx = git__calloc(1, sizeof(git_indexer_stream));
	GITERR_CHECK_ALLOC(idx);
	idx->progress_cb = progress_cb;
	idx->progress_payload = progress_payload;

	error = git_buf_joinpath(&path, prefix, suff);
	if (error < 0)
		goto cleanup;

	error = git_filebuf_open(&idx->pack_file, path.ptr,
				 GIT_FILEBUF_TEMPORARY | GIT_FILEBUF_DO_NOT_BUFFER);
	git_buf_free(&path);
	if (error < 0)
		goto cleanup;

	*out = idx;
	return 0;

cleanup:
	git_buf_free(&path);
	git_filebuf_cleanup(&idx->pack_file);
	git__free(idx);
	return -1;
}

/* Try to store the delta so we can try to resolve it later */
static int store_delta(git_indexer_stream *idx)
{
	struct delta_info *delta;

	delta = git__calloc(1, sizeof(struct delta_info));
	GITERR_CHECK_ALLOC(delta);
	delta->delta_off = idx->entry_start;

	if (git_vector_insert(&idx->deltas, delta) < 0)
		return -1;

	return 0;
}

static void hash_header(git_hash_ctx *ctx, git_off_t len, git_otype type)
{
	char buffer[64];
	size_t hdrlen;

	hdrlen = git_odb__format_object_header(buffer, sizeof(buffer), (size_t)len, type);
	git_hash_update(ctx, buffer, hdrlen);
}

static int hash_object_stream(git_indexer_stream *idx, git_packfile_stream *stream)
{
	ssize_t read;

	assert(idx && stream);

	do {
		if ((read = git_packfile_stream_read(stream, idx->objbuf, sizeof(idx->objbuf))) < 0)
			break;

		git_hash_update(&idx->hash_ctx, idx->objbuf, read);
	} while (read > 0);

	if (read < 0)
		return (int)read;

	return 0;
}

/* In order to create the packfile stream, we need to skip over the delta base description */
static int advance_delta_offset(git_indexer_stream *idx, git_otype type)
{
	git_mwindow *w = NULL;

	assert(type == GIT_OBJ_REF_DELTA || type == GIT_OBJ_OFS_DELTA);

	if (type == GIT_OBJ_REF_DELTA) {
		idx->off += GIT_OID_RAWSZ;
	} else {
		git_off_t base_off = get_delta_base(idx->pack, &w, &idx->off, type, idx->entry_start);
		git_mwindow_close(&w);
		if (base_off < 0)
			return (int)base_off;
	}

	return 0;
}

/* Read from the stream and discard any output */
static int read_object_stream(git_indexer_stream *idx, git_packfile_stream *stream)
{
	ssize_t read;

	assert(stream);

	do {
		read = git_packfile_stream_read(stream, idx->objbuf, sizeof(idx->objbuf));
	} while (read > 0);

	if (read < 0)
		return (int)read;

	return 0;
}

static int crc_object(uint32_t *crc_out, git_mwindow_file *mwf, git_off_t start, git_off_t size)
{
	void *ptr;
	uint32_t crc;
	unsigned int left, len;
	git_mwindow *w = NULL;

	crc = crc32(0L, Z_NULL, 0);
	while (size) {
		ptr = git_mwindow_open(mwf, &w, start, (size_t)size, &left);
		if (ptr == NULL)
			return -1;

		len = min(left, (unsigned int)size);
		crc = crc32(crc, ptr, len);
		size -= len;
		start += len;
		git_mwindow_close(&w);
	}

	*crc_out = htonl(crc);
	return 0;
}

static int store_object(git_indexer_stream *idx)
{
	int i;
	git_oid oid;
	struct entry *entry;
	git_off_t entry_size;
	struct git_pack_entry *pentry;
	git_hash_ctx *ctx = &idx->hash_ctx;
	git_off_t entry_start = idx->entry_start;

	entry = git__calloc(1, sizeof(*entry));
	GITERR_CHECK_ALLOC(entry);

	pentry = git__malloc(sizeof(struct git_pack_entry));
	GITERR_CHECK_ALLOC(pentry);

	git_hash_final(&oid, ctx);
	entry_size = idx->off - entry_start;
	if (entry_start > UINT31_MAX) {
		entry->offset = UINT32_MAX;
		entry->offset_long = entry_start;
	} else {
		entry->offset = (uint32_t)entry_start;
	}

	git_oid_cpy(&pentry->sha1, &oid);
	pentry->offset = entry_start;
	if (git_vector_insert(&idx->pack->cache, pentry) < 0) {
		git__free(pentry);
		goto on_error;
	}

	git_oid_cpy(&entry->oid, &oid);

	if (crc_object(&entry->crc, &idx->pack->mwf, entry_start, entry_size) < 0)
		goto on_error;

	/* Add the object to the list */
	if (git_vector_insert(&idx->objects, entry) < 0)
		goto on_error;

	for (i = oid.id[0]; i < 256; ++i) {
		idx->fanout[i]++;
	}

	return 0;

on_error:
	git__free(entry);

	return -1;
}

static int hash_and_save(git_indexer_stream *idx, git_rawobj *obj, git_off_t entry_start)
{
	int i;
	git_oid oid;
	size_t entry_size;
	struct entry *entry;
	struct git_pack_entry *pentry;

	entry = git__calloc(1, sizeof(*entry));
	GITERR_CHECK_ALLOC(entry);

	if (entry_start > UINT31_MAX) {
		entry->offset = UINT32_MAX;
		entry->offset_long = entry_start;
	} else {
		entry->offset = (uint32_t)entry_start;
	}

	/* FIXME: Parse the object instead of hashing it */
	if (git_odb__hashobj(&oid, obj) < 0) {
		giterr_set(GITERR_INDEXER, "Failed to hash object");
		return -1;
	}

	pentry = git__malloc(sizeof(struct git_pack_entry));
	GITERR_CHECK_ALLOC(pentry);

	git_oid_cpy(&pentry->sha1, &oid);
	pentry->offset = entry_start;
	if (git_vector_insert(&idx->pack->cache, pentry) < 0) {
		git__free(pentry);
		goto on_error;
	}

	git_oid_cpy(&entry->oid, &oid);
	entry->crc = crc32(0L, Z_NULL, 0);

	entry_size = (size_t)(idx->off - entry_start);
	if (crc_object(&entry->crc, &idx->pack->mwf, entry_start, entry_size) < 0)
		goto on_error;

	/* Add the object to the list */
	if (git_vector_insert(&idx->objects, entry) < 0)
		goto on_error;

	for (i = oid.id[0]; i < 256; ++i) {
		idx->fanout[i]++;
	}

	return 0;

on_error:
	git__free(entry);
	git__free(obj->data);
	return -1;
}

static int do_progress_callback(git_indexer_stream *idx, git_transfer_progress *stats)
{
	if (!idx->progress_cb) return 0;
	return idx->progress_cb(stats, idx->progress_payload);
}

int git_indexer_stream_add(git_indexer_stream *idx, const void *data, size_t size, git_transfer_progress *stats)
{
	int error = -1;
	struct git_pack_header hdr;
	size_t processed; 
	git_mwindow_file *mwf = &idx->pack->mwf;

	assert(idx && data && stats);

	processed = stats->indexed_objects;

	if (git_filebuf_write(&idx->pack_file, data, size) < 0)
		return -1;

	/* Make sure we set the new size of the pack */
	if (idx->opened_pack) {
		idx->pack->mwf.size += size;
		//printf("\nadding %zu for %zu\n", size, idx->pack->mwf.size);
	} else {
		if (open_pack(&idx->pack, idx->pack_file.path_lock) < 0)
			return -1;
		idx->opened_pack = 1;
		mwf = &idx->pack->mwf;
		if (git_mwindow_file_register(&idx->pack->mwf) < 0)
			return -1;
	}

	if (!idx->parsed_header) {
		if ((unsigned)idx->pack->mwf.size < sizeof(hdr))
			return 0;

		if (parse_header(&hdr, idx->pack) < 0)
			return -1;

		idx->parsed_header = 1;
		idx->nr_objects = ntohl(hdr.hdr_entries);
		idx->off = sizeof(struct git_pack_header);

		/* for now, limit to 2^32 objects */
		assert(idx->nr_objects == (size_t)((unsigned int)idx->nr_objects));

		if (git_vector_init(&idx->pack->cache, (unsigned int)idx->nr_objects, cache_cmp) < 0)
			return -1;

		idx->pack->has_cache = 1;
		if (git_vector_init(&idx->objects, (unsigned int)idx->nr_objects, objects_cmp) < 0)
			return -1;

		if (git_vector_init(&idx->deltas, (unsigned int)(idx->nr_objects / 2), NULL) < 0)
			return -1;

		stats->received_objects = 0;
		stats->indexed_objects = 0;
		stats->total_objects = (unsigned int)idx->nr_objects;
		do_progress_callback(idx, stats);
	}

	/* Now that we have data in the pack, let's try to parse it */

	/* As the file grows any windows we try to use will be out of date */
	git_mwindow_free_all(mwf);
	while (processed < idx->nr_objects) {
		git_packfile_stream *stream = &idx->stream;
		git_off_t entry_start = idx->off;
		size_t entry_size;
		git_otype type;
		git_mwindow *w = NULL;

		if (idx->pack->mwf.size <= idx->off + 20)
			return 0;

		if (!idx->have_stream) {
			error = git_packfile_unpack_header(&entry_size, &type, mwf, &w, &idx->off);
			if (error == GIT_EBUFS) {
				idx->off = entry_start;
				return 0;
			}
			if (error < 0)
				return -1;

			git_mwindow_close(&w);
			idx->entry_start = entry_start;
			git_hash_ctx_init(&idx->hash_ctx);

			if (type == GIT_OBJ_REF_DELTA || type == GIT_OBJ_OFS_DELTA) {
				error = advance_delta_offset(idx, type);
				if (error == GIT_EBUFS) {
					idx->off = entry_start;
					return 0;
				}
				if (error < 0)
					return -1;

				idx->have_delta = 1;
			} else {
				idx->have_delta = 0;
				hash_header(&idx->hash_ctx, entry_size, type);
			}

			idx->have_stream = 1;
			if (git_packfile_stream_open(stream, idx->pack, idx->off) < 0)
				goto on_error;

		}

		if (idx->have_delta) {
			error = read_object_stream(idx, stream);
		} else {
			error = hash_object_stream(idx, stream);
		}

		idx->off = stream->curpos;
		if (error == GIT_EBUFS)
			return 0;

		/* We want to free the stream reasorces no matter what here */
		idx->have_stream = 0;
		git_packfile_stream_free(stream);

		if (error < 0)
			goto on_error;

		if (idx->have_delta) {
			error = store_delta(idx);
		} else {
			error = store_object(idx);
		}

		if (error < 0)
			goto on_error;

		if (!idx->have_delta) {
			stats->indexed_objects = (unsigned int)++processed;
		}
		stats->received_objects++;

		if (do_progress_callback(idx, stats) != 0) {
			error = GIT_EUSER;
			goto on_error;
		}
	}

	return 0;

on_error:
	git_mwindow_free_all(mwf);
	return error;
}

static int index_path_stream(git_buf *path, git_indexer_stream *idx, const char *suffix)
{
	const char prefix[] = "pack-";
	size_t slash = (size_t)path->size;

	/* search backwards for '/' */
	while (slash > 0 && path->ptr[slash - 1] != '/')
		slash--;

	if (git_buf_grow(path, slash + 1 + strlen(prefix) +
					 GIT_OID_HEXSZ + strlen(suffix) + 1) < 0)
		return -1;

	git_buf_truncate(path, slash);
	git_buf_puts(path, prefix);
	git_oid_fmt(path->ptr + git_buf_len(path), &idx->hash);
	path->size += GIT_OID_HEXSZ;
	git_buf_puts(path, suffix);

	return git_buf_oom(path) ? -1 : 0;
}

static int resolve_deltas(git_indexer_stream *idx, git_transfer_progress *stats)
{
	unsigned int i;
	struct delta_info *delta;

	git_vector_foreach(&idx->deltas, i, delta) {
		git_rawobj obj;

		idx->off = delta->delta_off;
		if (git_packfile_unpack(&obj, idx->pack, &idx->off) < 0)
			return -1;

		if (hash_and_save(idx, &obj, delta->delta_off) < 0)
			return -1;

		git__free(obj.data);
		stats->indexed_objects++;
		do_progress_callback(idx, stats);
	}

	return 0;
}

int git_indexer_stream_finalize(git_indexer_stream *idx, git_transfer_progress *stats)
{
	git_mwindow *w = NULL;
	unsigned int i, long_offsets = 0, left;
	struct git_pack_idx_header hdr;
	git_buf filename = GIT_BUF_INIT;
	struct entry *entry;
	void *packfile_hash;
	git_oid file_hash;
	git_hash_ctx ctx;
	git_filebuf index_file = {0};

	if (git_hash_ctx_init(&ctx) < 0)
		return -1;

	/* Test for this before resolve_deltas(), as it plays with idx->off */
	if (idx->off < idx->pack->mwf.size - GIT_OID_RAWSZ) {
		giterr_set(GITERR_INDEXER, "Indexing error: unexpected data at the end of the pack");
		return -1;
	}

	if (idx->deltas.length > 0)
		if (resolve_deltas(idx, stats) < 0)
			return -1;

	if (stats->indexed_objects != stats->total_objects) {
		giterr_set(GITERR_INDEXER, "Indexing error: early EOF");
		return -1;
	}

	git_vector_sort(&idx->objects);

	git_buf_sets(&filename, idx->pack->pack_name);
	git_buf_truncate(&filename, filename.size - strlen("pack"));
	git_buf_puts(&filename, "idx");
	if (git_buf_oom(&filename))
		return -1;

	if (git_filebuf_open(&index_file, filename.ptr, GIT_FILEBUF_HASH_CONTENTS) < 0)
		goto on_error;

	/* Write out the header */
	hdr.idx_signature = htonl(PACK_IDX_SIGNATURE);
	hdr.idx_version = htonl(2);
	git_filebuf_write(&index_file, &hdr, sizeof(hdr));

	/* Write out the fanout table */
	for (i = 0; i < 256; ++i) {
		uint32_t n = htonl(idx->fanout[i]);
		git_filebuf_write(&index_file, &n, sizeof(n));
	}

	/* Write out the object names (SHA-1 hashes) */
	git_vector_foreach(&idx->objects, i, entry) {
		git_filebuf_write(&index_file, &entry->oid, sizeof(git_oid));
		git_hash_update(&ctx, &entry->oid, GIT_OID_RAWSZ);
	}
	git_hash_final(&idx->hash, &ctx);

	/* Write out the CRC32 values */
	git_vector_foreach(&idx->objects, i, entry) {
		git_filebuf_write(&index_file, &entry->crc, sizeof(uint32_t));
	}

	/* Write out the offsets */
	git_vector_foreach(&idx->objects, i, entry) {
		uint32_t n;

		if (entry->offset == UINT32_MAX)
			n = htonl(0x80000000 | long_offsets++);
		else
			n = htonl(entry->offset);

		git_filebuf_write(&index_file, &n, sizeof(uint32_t));
	}

	/* Write out the long offsets */
	git_vector_foreach(&idx->objects, i, entry) {
		uint32_t split[2];

		if (entry->offset != UINT32_MAX)
			continue;

		split[0] = htonl(entry->offset_long >> 32);
		split[1] = htonl(entry->offset_long & 0xffffffff);

		git_filebuf_write(&index_file, &split, sizeof(uint32_t) * 2);
	}

	/* Write out the packfile trailer */
	packfile_hash = git_mwindow_open(&idx->pack->mwf, &w, idx->pack->mwf.size - GIT_OID_RAWSZ, GIT_OID_RAWSZ, &left);
	if (packfile_hash == NULL) {
		git_mwindow_close(&w);
		goto on_error;
	}

	memcpy(&file_hash, packfile_hash, GIT_OID_RAWSZ);
	git_mwindow_close(&w);

	git_filebuf_write(&index_file, &file_hash, sizeof(git_oid));

	/* Write out the packfile trailer to the idx file as well */
	if (git_filebuf_hash(&file_hash, &index_file) < 0)
		goto on_error;

	git_filebuf_write(&index_file, &file_hash, sizeof(git_oid));

	/* Figure out what the final name should be */
	if (index_path_stream(&filename, idx, ".idx") < 0)
		goto on_error;

	/* Commit file */
	if (git_filebuf_commit_at(&index_file, filename.ptr, GIT_PACK_FILE_MODE) < 0)
		goto on_error;

	git_mwindow_free_all(&idx->pack->mwf);
	/* We need to close the descriptor here so Windows doesn't choke on commit_at */
	p_close(idx->pack->mwf.fd);
	idx->pack->mwf.fd = -1;

	if (index_path_stream(&filename, idx, ".pack") < 0)
		goto on_error;
	/* And don't forget to rename the packfile to its new place. */
	if (git_filebuf_commit_at(&idx->pack_file, filename.ptr, GIT_PACK_FILE_MODE) < 0)
		return -1;

	git_buf_free(&filename);
	return 0;

on_error:
	git_mwindow_free_all(&idx->pack->mwf);
	git_filebuf_cleanup(&index_file);
	git_buf_free(&filename);
	git_hash_ctx_cleanup(&ctx);
	return -1;
}

void git_indexer_stream_free(git_indexer_stream *idx)
{
	unsigned int i;
	struct entry *e;
	struct git_pack_entry *pe;
	struct delta_info *delta;

	if (idx == NULL)
		return;

	git_vector_foreach(&idx->objects, i, e)
		git__free(e);
	git_vector_free(&idx->objects);
	if (idx->pack) {
		git_vector_foreach(&idx->pack->cache, i, pe)
			git__free(pe);
		git_vector_free(&idx->pack->cache);
	}
	git_vector_foreach(&idx->deltas, i, delta)
		git__free(delta);
	git_vector_free(&idx->deltas);
	git_packfile_free(idx->pack);
	git_filebuf_cleanup(&idx->pack_file);
	git__free(idx);
}

int git_indexer_new(git_indexer **out, const char *packname)
{
	git_indexer *idx;
	struct git_pack_header hdr;
	int error;

	assert(out && packname);

	idx = git__calloc(1, sizeof(git_indexer));
	GITERR_CHECK_ALLOC(idx);

	open_pack(&idx->pack, packname);

	if ((error = parse_header(&hdr, idx->pack)) < 0)
		goto cleanup;

	idx->nr_objects = ntohl(hdr.hdr_entries);

	/* for now, limit to 2^32 objects */
	assert(idx->nr_objects == (size_t)((unsigned int)idx->nr_objects));

	error = git_vector_init(&idx->pack->cache, (unsigned int)idx->nr_objects, cache_cmp);
	if (error < 0)
		goto cleanup;

	idx->pack->has_cache = 1;
	error = git_vector_init(&idx->objects, (unsigned int)idx->nr_objects, objects_cmp);
	if (error < 0)
		goto cleanup;

	*out = idx;

	return 0;

cleanup:
	git_indexer_free(idx);

	return -1;
}

static int index_path(git_buf *path, git_indexer *idx)
{
	const char prefix[] = "pack-", suffix[] = ".idx";
	size_t slash = (size_t)path->size;

	/* search backwards for '/' */
	while (slash > 0 && path->ptr[slash - 1] != '/')
		slash--;

	if (git_buf_grow(path, slash + 1 + strlen(prefix) +
					 GIT_OID_HEXSZ + strlen(suffix) + 1) < 0)
		return -1;

	git_buf_truncate(path, slash);
	git_buf_puts(path, prefix);
	git_oid_fmt(path->ptr + git_buf_len(path), &idx->hash);
	path->size += GIT_OID_HEXSZ;
	git_buf_puts(path, suffix);

	return git_buf_oom(path) ? -1 : 0;
}

int git_indexer_write(git_indexer *idx)
{
	git_mwindow *w = NULL;
	int error;
	unsigned int i, long_offsets = 0, left;
	struct git_pack_idx_header hdr;
	git_buf filename = GIT_BUF_INIT;
	struct entry *entry;
	void *packfile_hash;
	git_oid file_hash;
	git_hash_ctx ctx;

	if (git_hash_ctx_init(&ctx) < 0)
		return -1;

	git_vector_sort(&idx->objects);

	git_buf_sets(&filename, idx->pack->pack_name);
	git_buf_truncate(&filename, filename.size - strlen("pack"));
	git_buf_puts(&filename, "idx");
	if (git_buf_oom(&filename))
		return -1;

	error = git_filebuf_open(&idx->file, filename.ptr, GIT_FILEBUF_HASH_CONTENTS);
	if (error < 0)
		goto cleanup;

	/* Write out the header */
	hdr.idx_signature = htonl(PACK_IDX_SIGNATURE);
	hdr.idx_version = htonl(2);
	error = git_filebuf_write(&idx->file, &hdr, sizeof(hdr));
	if (error < 0)
		goto cleanup;

	/* Write out the fanout table */
	for (i = 0; i < 256; ++i) {
		uint32_t n = htonl(idx->fanout[i]);
		error = git_filebuf_write(&idx->file, &n, sizeof(n));
		if (error < 0)
			goto cleanup;
	}

	/* Write out the object names (SHA-1 hashes) */
	git_vector_foreach(&idx->objects, i, entry) {
		if ((error = git_filebuf_write(&idx->file, &entry->oid, sizeof(git_oid))) < 0 ||
			(error = git_hash_update(&ctx, &entry->oid, GIT_OID_RAWSZ)) < 0)
			goto cleanup;
	}

	if ((error = git_hash_final(&idx->hash, &ctx)) < 0)
		goto cleanup;

	/* Write out the CRC32 values */
	git_vector_foreach(&idx->objects, i, entry) {
		error = git_filebuf_write(&idx->file, &entry->crc, sizeof(uint32_t));
		if (error < 0)
			goto cleanup;
	}

	/* Write out the offsets */
	git_vector_foreach(&idx->objects, i, entry) {
		uint32_t n;

		if (entry->offset == UINT32_MAX)
			n = htonl(0x80000000 | long_offsets++);
		else
			n = htonl(entry->offset);

		error = git_filebuf_write(&idx->file, &n, sizeof(uint32_t));
		if (error < 0)
			goto cleanup;
	}

	/* Write out the long offsets */
	git_vector_foreach(&idx->objects, i, entry) {
		uint32_t split[2];

		if (entry->offset != UINT32_MAX)
			continue;

		split[0] = htonl(entry->offset_long >> 32);
		split[1] = htonl(entry->offset_long & 0xffffffff);

		error = git_filebuf_write(&idx->file, &split, sizeof(uint32_t) * 2);
		if (error < 0)
			goto cleanup;
	}

	/* Write out the packfile trailer */

	packfile_hash = git_mwindow_open(&idx->pack->mwf, &w, idx->pack->mwf.size - GIT_OID_RAWSZ, GIT_OID_RAWSZ, &left);
	git_mwindow_close(&w);
	if (packfile_hash == NULL) {
		error = -1;
		goto cleanup;
	}

	memcpy(&file_hash, packfile_hash, GIT_OID_RAWSZ);

	git_mwindow_close(&w);

	error = git_filebuf_write(&idx->file, &file_hash, sizeof(git_oid));
	if (error < 0)
		goto cleanup;

	/* Write out the index sha */
	error = git_filebuf_hash(&file_hash, &idx->file);
	if (error < 0)
		goto cleanup;

	error = git_filebuf_write(&idx->file, &file_hash, sizeof(git_oid));
	if (error < 0)
		goto cleanup;

	/* Figure out what the final name should be */
	error = index_path(&filename, idx);
	if (error < 0)
		goto cleanup;

	/* Commit file */
	error = git_filebuf_commit_at(&idx->file, filename.ptr, GIT_PACK_FILE_MODE);

cleanup:
	git_mwindow_free_all(&idx->pack->mwf);
	git_mwindow_file_deregister(&idx->pack->mwf);
	if (error < 0)
		git_filebuf_cleanup(&idx->file);
	git_buf_free(&filename);
	git_hash_ctx_cleanup(&ctx);

	return error;
}

int git_indexer_run(git_indexer *idx, git_transfer_progress *stats)
{
	git_mwindow_file *mwf;
	git_off_t off = sizeof(struct git_pack_header);
	int error;
	struct entry *entry;
	unsigned int left, processed;

	assert(idx && stats);

	mwf = &idx->pack->mwf;
	error = git_mwindow_file_register(mwf);
	if (error < 0)
		return error;

	stats->total_objects = (unsigned int)idx->nr_objects;
	stats->indexed_objects = processed = 0;

	while (processed < idx->nr_objects) {
		git_rawobj obj;
		git_oid oid;
		struct git_pack_entry *pentry;
		git_mwindow *w = NULL;
		int i;
		git_off_t entry_start = off;
		void *packed;
		size_t entry_size;
		char fmt[GIT_OID_HEXSZ] = {0};

		entry = git__calloc(1, sizeof(*entry));
		GITERR_CHECK_ALLOC(entry);

		if (off > UINT31_MAX) {
			entry->offset = UINT32_MAX;
			entry->offset_long = off;
		} else {
			entry->offset = (uint32_t)off;
		}

		error = git_packfile_unpack(&obj, idx->pack, &off);
		if (error < 0)
			goto cleanup;

		/* FIXME: Parse the object instead of hashing it */
		error = git_odb__hashobj(&oid, &obj);
		if (error < 0) {
			giterr_set(GITERR_INDEXER, "Failed to hash object");
			goto cleanup;
		}

		pentry = git__malloc(sizeof(struct git_pack_entry));
		if (pentry == NULL) {
			error = -1;
			goto cleanup;
		}

		git_oid_cpy(&pentry->sha1, &oid);
		pentry->offset = entry_start;
		git_oid_fmt(fmt, &oid);
		error = git_vector_insert(&idx->pack->cache, pentry);
		if (error < 0)
			goto cleanup;

		git_oid_cpy(&entry->oid, &oid);
		entry->crc = crc32(0L, Z_NULL, 0);

		entry_size = (size_t)(off - entry_start);
		packed = git_mwindow_open(mwf, &w, entry_start, entry_size, &left);
		if (packed == NULL) {
			error = -1;
			goto cleanup;
		}
		entry->crc = htonl(crc32(entry->crc, packed, (uInt)entry_size));
		git_mwindow_close(&w);

		/* Add the object to the list */
		error = git_vector_insert(&idx->objects, entry);
		if (error < 0)
			goto cleanup;

		for (i = oid.id[0]; i < 256; ++i) {
			idx->fanout[i]++;
		}

		git__free(obj.data);

		stats->indexed_objects = ++processed;
	}

cleanup:
	git_mwindow_free_all(mwf);

	return error;

}

void git_indexer_free(git_indexer *idx)
{
	unsigned int i;
	struct entry *e;
	struct git_pack_entry *pe;

	if (idx == NULL)
		return;

	git_mwindow_file_deregister(&idx->pack->mwf);
	git_vector_foreach(&idx->objects, i, e)
		git__free(e);
	git_vector_free(&idx->objects);
	git_vector_foreach(&idx->pack->cache, i, pe)
		git__free(pe);
	git_vector_free(&idx->pack->cache);
	git_packfile_free(idx->pack);
	git__free(idx);
}

