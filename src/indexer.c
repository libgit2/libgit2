/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "git2/indexer.h"
#include "git2/object.h"
#include "git2/zlib.h"
#include "git2/oid.h"

#include "common.h"
#include "pack.h"
#include "mwindow.h"
#include "posix.h"
#include "pack.h"
#include "filebuf.h"
#include "sha1.h"

#define UINT31_MAX (0x7FFFFFFF)

struct entry {
	git_oid oid;
	uint32_t crc;
	uint32_t offset;
	uint64_t offset_long;
};

struct git_indexer {
	struct git_pack_file *pack;
	struct stat st;
	struct git_pack_header hdr;
	size_t nr_objects;
	git_vector objects;
	git_filebuf file;
	unsigned int fanout[256];
	git_oid hash;
};

const git_oid *git_indexer_hash(git_indexer *idx)
{
	return &idx->hash;
}

static int parse_header(git_indexer *idx)
{
	int error;

	/* Verify we recognize this pack file format. */
	if ((error = p_read(idx->pack->mwf.fd, &idx->hdr, sizeof(idx->hdr))) < GIT_SUCCESS)
		return git__rethrow(error, "Failed to read in pack header");

	if (idx->hdr.hdr_signature != ntohl(PACK_SIGNATURE))
		return git__throw(GIT_EOBJCORRUPTED, "Wrong pack signature");

	if (!pack_version_ok(idx->hdr.hdr_version))
		return git__throw(GIT_EOBJCORRUPTED, "Wrong pack version");


	return GIT_SUCCESS;
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


int git_indexer_new(git_indexer **out, const char *packname)
{
	git_indexer *idx;
	size_t namelen;
	int ret, error;

	assert(out && packname);

	if (git_path_root(packname) < 0)
		return git__throw(GIT_EINVALIDPATH, "Path is not absolute");

	idx = git__malloc(sizeof(git_indexer));
	if (idx == NULL)
		return GIT_ENOMEM;

	memset(idx, 0x0, sizeof(*idx));

	namelen = strlen(packname);
	idx->pack = git__malloc(sizeof(struct git_pack_file) + namelen + 1);
	if (idx->pack == NULL) {
		error = GIT_ENOMEM;
		goto cleanup;
	}

	memset(idx->pack, 0x0, sizeof(struct git_pack_file));
	memcpy(idx->pack->pack_name, packname, namelen + 1);

	ret = p_stat(packname, &idx->st);
	if (ret < 0) {
		if (errno == ENOENT)
			error = git__throw(GIT_ENOTFOUND, "Failed to stat packfile. File not found");
		else
			error = git__throw(GIT_EOSERR, "Failed to stat packfile.");

		goto cleanup;
	}

	ret = p_open(idx->pack->pack_name, O_RDONLY);
	if (ret < 0) {
		error = git__throw(GIT_EOSERR, "Failed to open packfile");
		goto cleanup;
	}

	idx->pack->mwf.fd = ret;
	idx->pack->mwf.size = (git_off_t)idx->st.st_size;

	error = parse_header(idx);
	if (error < GIT_SUCCESS) {
		error = git__rethrow(error, "Failed to parse packfile header");
		goto cleanup;
	}

	idx->nr_objects = ntohl(idx->hdr.hdr_entries);

	error = git_vector_init(&idx->pack->cache, idx->nr_objects, cache_cmp);
	if (error < GIT_SUCCESS)
		goto cleanup;

	idx->pack->has_cache = 1;
	error = git_vector_init(&idx->objects, idx->nr_objects, objects_cmp);
	if (error < GIT_SUCCESS)
		goto cleanup;

	*out = idx;

	return GIT_SUCCESS;

cleanup:
	git_indexer_free(idx);

	return error;
}

static void index_path(char *path, git_indexer *idx)
{
	char *ptr;
	const char prefix[] = "pack-", suffix[] = ".idx";

	ptr = strrchr(path, '/') + 1;

	memcpy(ptr, prefix, strlen(prefix));
	ptr += strlen(prefix);
	git_oid_fmt(ptr, &idx->hash);
	ptr += GIT_OID_HEXSZ;
	memcpy(ptr, suffix, strlen(suffix) + 1);
}

int git_indexer_write(git_indexer *idx)
{
	git_mwindow *w = NULL;
	int error;
	size_t namelen;
	unsigned int i, long_offsets = 0, left;
	struct git_pack_idx_header hdr;
	char filename[GIT_PATH_MAX];
	struct entry *entry;
	void *packfile_hash;
	git_oid file_hash;
	SHA_CTX ctx;

	git_vector_sort(&idx->objects);

	namelen = strlen(idx->pack->pack_name);
	memcpy(filename, idx->pack->pack_name, namelen);
	memcpy(filename + namelen - strlen("pack"), "idx", strlen("idx") + 1);

	error = git_filebuf_open(&idx->file, filename, GIT_FILEBUF_HASH_CONTENTS);

	/* Write out the header */
	hdr.idx_signature = htonl(PACK_IDX_SIGNATURE);
	hdr.idx_version = htonl(2);
	error = git_filebuf_write(&idx->file, &hdr, sizeof(hdr));

	/* Write out the fanout table */
	for (i = 0; i < 256; ++i) {
		uint32_t n = htonl(idx->fanout[i]);
		error = git_filebuf_write(&idx->file, &n, sizeof(n));
		if (error < GIT_SUCCESS)
			goto cleanup;
	}

	/* Write out the object names (SHA-1 hashes) */
	SHA1_Init(&ctx);
	git_vector_foreach(&idx->objects, i, entry) {
		error = git_filebuf_write(&idx->file, &entry->oid, sizeof(git_oid));
		SHA1_Update(&ctx, &entry->oid, GIT_OID_RAWSZ);
		if (error < GIT_SUCCESS)
			goto cleanup;
	}
	SHA1_Final(idx->hash.id, &ctx);

	/* Write out the CRC32 values */
	git_vector_foreach(&idx->objects, i, entry) {
		error = git_filebuf_write(&idx->file, &entry->crc, sizeof(uint32_t));
		if (error < GIT_SUCCESS)
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
		if (error < GIT_SUCCESS)
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
		if (error < GIT_SUCCESS)
			goto cleanup;
	}

	/* Write out the packfile trailer */

	packfile_hash = git_mwindow_open(&idx->pack->mwf, &w, idx->st.st_size - GIT_OID_RAWSZ, GIT_OID_RAWSZ, &left);
	git_mwindow_close(&w);
	if (packfile_hash == NULL) {
		error = git__rethrow(GIT_ENOMEM, "Failed to open window to packfile hash");
		goto cleanup;
	}

	memcpy(&file_hash, packfile_hash, GIT_OID_RAWSZ);

	git_mwindow_close(&w);

	error = git_filebuf_write(&idx->file, &file_hash, sizeof(git_oid));

	/* Write out the index sha */
	error = git_filebuf_hash(&file_hash, &idx->file);
	if (error < GIT_SUCCESS)
		goto cleanup;

	error = git_filebuf_write(&idx->file, &file_hash, sizeof(git_oid));
	if (error < GIT_SUCCESS)
		goto cleanup;

	/* Figure out what the final name should be */
	index_path(filename, idx);
	/* Commit file */
	error = git_filebuf_commit_at(&idx->file, filename, GIT_PACK_FILE_MODE);

cleanup:
	git_mwindow_free_all(&idx->pack->mwf);
	if (error < GIT_SUCCESS)
		git_filebuf_cleanup(&idx->file);

	return error;
}

int git_indexer_run(git_indexer *idx, git_indexer_stats *stats)
{
	git_mwindow_file *mwf;
	off_t off = sizeof(struct git_pack_header);
	int error;
	struct entry *entry;
	unsigned int left, processed;

	assert(idx && stats);

	mwf = &idx->pack->mwf;
	error = git_mwindow_file_register(mwf);
	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to register mwindow file");

	stats->total = idx->nr_objects;
	stats->processed = processed = 0;

	while (processed < idx->nr_objects) {
		git_rawobj obj;
		git_oid oid;
		struct git_pack_entry *pentry;
		git_mwindow *w = NULL;
		int i;
		off_t entry_start = off;
		void *packed;
		size_t entry_size;

		entry = git__malloc(sizeof(struct entry));
		memset(entry, 0x0, sizeof(struct entry));

		if (off > UINT31_MAX) {
			entry->offset = UINT32_MAX;
			entry->offset_long = off;
		} else {
			entry->offset = off;
		}

		error = git_packfile_unpack(&obj, idx->pack, &off);
		if (error < GIT_SUCCESS) {
			error = git__rethrow(error, "Failed to unpack object");
			goto cleanup;
		}

		/* FIXME: Parse the object instead of hashing it */
		error = git_odb__hash_obj(&oid, &obj);
		if (error < GIT_SUCCESS) {
			error = git__rethrow(error, "Failed to hash object");
			goto cleanup;
		}

		pentry = git__malloc(sizeof(struct git_pack_entry));
		if (pentry == NULL) {
			error = GIT_ENOMEM;
			goto cleanup;
		}
		git_oid_cpy(&pentry->sha1, &oid);
		pentry->offset = entry_start;
		error = git_vector_insert(&idx->pack->cache, pentry);
		if (error < GIT_SUCCESS)
			goto cleanup;

		git_oid_cpy(&entry->oid, &oid);
		entry->crc = crc32(0L, Z_NULL, 0);

		entry_size = off - entry_start;
		packed = git_mwindow_open(mwf, &w, entry_start, entry_size, &left);
		if (packed == NULL) {
			error = git__rethrow(error, "Failed to open window to read packed data");
			goto cleanup;
		}
		entry->crc = htonl(crc32(entry->crc, packed, entry_size));
		git_mwindow_close(&w);

		/* Add the object to the list */
		error = git_vector_insert(&idx->objects, entry);
		if (error < GIT_SUCCESS) {
			error = git__rethrow(error, "Failed to add entry to list");
			goto cleanup;
		}

		for (i = oid.id[0]; i < 256; ++i) {
			idx->fanout[i]++;
		}

		git__free(obj.data);

		stats->processed = ++processed;
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

	p_close(idx->pack->mwf.fd);
	git_vector_foreach(&idx->objects, i, e)
		git__free(e);
	git_vector_free(&idx->objects);
	git_vector_foreach(&idx->pack->cache, i, pe)
		git__free(pe);
	git_vector_free(&idx->pack->cache);
	git__free(idx->pack);
	git__free(idx);
}

