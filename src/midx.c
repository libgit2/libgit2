/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "midx.h"

#include "buffer.h"
#include "futils.h"
#include "hash.h"
#include "odb.h"
#include "pack.h"

#define MIDX_SIGNATURE 0x4d494458 /* "MIDX" */
#define MIDX_VERSION 1
#define MIDX_OBJECT_ID_VERSION 1
struct git_midx_header {
	uint32_t signature;
	uint8_t version;
	uint8_t object_id_version;
	uint8_t chunks;
	uint8_t base_midx_files;
	uint32_t packfiles;
};

#define MIDX_PACKFILE_NAMES_ID 0x504e414d	   /* "PNAM" */
#define MIDX_OID_FANOUT_ID 0x4f494446	   /* "OIDF" */
#define MIDX_OID_LOOKUP_ID 0x4f49444c	   /* "OIDL" */
#define MIDX_OBJECT_OFFSETS_ID 0x4f4f4646	   /* "OOFF" */
#define MIDX_OBJECT_LARGE_OFFSETS_ID 0x4c4f4646 /* "LOFF" */

struct git_midx_chunk {
	off64_t offset;
	size_t length;
};

static int midx_error(const char *message)
{
	git_error_set(GIT_ERROR_ODB, "invalid multi-pack-index file - %s", message);
	return -1;
}

static int midx_parse_packfile_names(
		git_midx_file *idx,
		const unsigned char *data,
		uint32_t packfiles,
		struct git_midx_chunk *chunk)
{
	int error;
	uint32_t i;
	char *packfile_name = (char *)(data + chunk->offset);
	size_t chunk_size = chunk->length, len;
	if (chunk->offset == 0)
		return midx_error("missing Packfile Names chunk");
	if (chunk->length == 0)
		return midx_error("empty Packfile Names chunk");
	if ((error = git_vector_init(&idx->packfile_names, packfiles, git__strcmp_cb)) < 0)
		return error;
	for (i = 0; i < packfiles; ++i) {
		len = p_strnlen(packfile_name, chunk_size);
		if (len == 0)
			return midx_error("empty packfile name");
		if (len + 1 > chunk_size)
			return midx_error("unterminated packfile name");
		git_vector_insert(&idx->packfile_names, packfile_name);
		if (i && strcmp(git_vector_get(&idx->packfile_names, i - 1), packfile_name) >= 0)
			return midx_error("packfile names are not sorted");
		if (strlen(packfile_name) <= strlen(".idx") || git__suffixcmp(packfile_name, ".idx") != 0)
			return midx_error("non-.idx packfile name");
		if (strchr(packfile_name, '/') != NULL || strchr(packfile_name, '\\') != NULL)
			return midx_error("non-local packfile");
		packfile_name += len + 1;
		chunk_size -= len + 1;
	}
	return 0;
}

static int midx_parse_oid_fanout(
		git_midx_file *idx,
		const unsigned char *data,
		struct git_midx_chunk *chunk_oid_fanout)
{
	uint32_t i, nr;
	if (chunk_oid_fanout->offset == 0)
		return midx_error("missing OID Fanout chunk");
	if (chunk_oid_fanout->length == 0)
		return midx_error("empty OID Fanout chunk");
	if (chunk_oid_fanout->length != 256 * 4)
		return midx_error("OID Fanout chunk has wrong length");

	idx->oid_fanout = (const uint32_t *)(data + chunk_oid_fanout->offset);
	nr = 0;
	for (i = 0; i < 256; ++i) {
		uint32_t n = ntohl(idx->oid_fanout[i]);
		if (n < nr)
			return midx_error("index is non-monotonic");
		nr = n;
	}
	idx->num_objects = nr;
	return 0;
}

static int midx_parse_oid_lookup(
		git_midx_file *idx,
		const unsigned char *data,
		struct git_midx_chunk *chunk_oid_lookup)
{
	uint32_t i;
	git_oid *oid, *prev_oid, zero_oid = {{0}};

	if (chunk_oid_lookup->offset == 0)
		return midx_error("missing OID Lookup chunk");
	if (chunk_oid_lookup->length == 0)
		return midx_error("empty OID Lookup chunk");
	if (chunk_oid_lookup->length != idx->num_objects * GIT_OID_RAWSZ)
		return midx_error("OID Lookup chunk has wrong length");

	idx->oid_lookup = oid = (git_oid *)(data + chunk_oid_lookup->offset);
	prev_oid = &zero_oid;
	for (i = 0; i < idx->num_objects; ++i, ++oid) {
		if (git_oid_cmp(prev_oid, oid) >= 0)
			return midx_error("OID Lookup index is non-monotonic");
		prev_oid = oid;
	}

	return 0;
}

static int midx_parse_object_offsets(
		git_midx_file *idx,
		const unsigned char *data,
		struct git_midx_chunk *chunk_object_offsets)
{
	if (chunk_object_offsets->offset == 0)
		return midx_error("missing Object Offsets chunk");
	if (chunk_object_offsets->length == 0)
		return midx_error("empty Object Offsets chunk");
	if (chunk_object_offsets->length != idx->num_objects * 8)
		return midx_error("Object Offsets chunk has wrong length");

	idx->object_offsets = data + chunk_object_offsets->offset;

	return 0;
}

static int midx_parse_object_large_offsets(
		git_midx_file *idx,
		const unsigned char *data,
		struct git_midx_chunk *chunk_object_large_offsets)
{
	if (chunk_object_large_offsets->length == 0)
		return 0;
	if (chunk_object_large_offsets->length % 8 != 0)
		return midx_error("malformed Object Large Offsets chunk");

	idx->object_large_offsets = data + chunk_object_large_offsets->offset;
	idx->num_object_large_offsets = chunk_object_large_offsets->length / 8;

	return 0;
}

int git_midx_parse(
		git_midx_file *idx,
		const unsigned char *data,
		size_t size)
{
	struct git_midx_header *hdr;
	const unsigned char *chunk_hdr;
	struct git_midx_chunk *last_chunk;
	uint32_t i;
	off64_t last_chunk_offset, chunk_offset, trailer_offset;
	git_oid idx_checksum = {{0}};
	int error;
	struct git_midx_chunk chunk_packfile_names = {0},
					 chunk_oid_fanout = {0},
					 chunk_oid_lookup = {0},
					 chunk_object_offsets = {0},
					 chunk_object_large_offsets = {0};

	GIT_ASSERT_ARG(idx);

	if (size < sizeof(struct git_midx_header) + GIT_OID_RAWSZ)
		return midx_error("multi-pack index is too short");

	hdr = ((struct git_midx_header *)data);

	if (hdr->signature != htonl(MIDX_SIGNATURE) ||
	    hdr->version != MIDX_VERSION ||
	    hdr->object_id_version != MIDX_OBJECT_ID_VERSION) {
		return midx_error("unsupported multi-pack index version");
	}
	if (hdr->chunks == 0)
		return midx_error("no chunks in multi-pack index");

	/*
	 * The very first chunk's offset should be after the header, all the chunk
	 * headers, and a special zero chunk.
	 */
	last_chunk_offset =
			sizeof(struct git_midx_header) +
			(1 + hdr->chunks) * 12;
	trailer_offset = size - GIT_OID_RAWSZ;
	if (trailer_offset < last_chunk_offset)
		return midx_error("wrong index size");
	git_oid_cpy(&idx->checksum, (git_oid *)(data + trailer_offset));

	if (git_hash_buf(&idx_checksum, data, (size_t)trailer_offset) < 0)
		return midx_error("could not calculate signature");
	if (!git_oid_equal(&idx_checksum, &idx->checksum))
		return midx_error("index signature mismatch");

	chunk_hdr = data + sizeof(struct git_midx_header);
	last_chunk = NULL;
	for (i = 0; i < hdr->chunks; ++i, chunk_hdr += 12) {
		chunk_offset = ((off64_t)ntohl(*((uint32_t *)(chunk_hdr + 4)))) << 32 |
				((off64_t)ntohl(*((uint32_t *)(chunk_hdr + 8))));
		if (chunk_offset < last_chunk_offset)
			return midx_error("chunks are non-monotonic");
		if (chunk_offset >= trailer_offset)
			return midx_error("chunks extend beyond the trailer");
		if (last_chunk != NULL)
			last_chunk->length = (size_t)(chunk_offset - last_chunk_offset);
		last_chunk_offset = chunk_offset;

		switch (ntohl(*((uint32_t *)(chunk_hdr + 0)))) {
		case MIDX_PACKFILE_NAMES_ID:
			chunk_packfile_names.offset = last_chunk_offset;
			last_chunk = &chunk_packfile_names;
			break;

		case MIDX_OID_FANOUT_ID:
			chunk_oid_fanout.offset = last_chunk_offset;
			last_chunk = &chunk_oid_fanout;
			break;

		case MIDX_OID_LOOKUP_ID:
			chunk_oid_lookup.offset = last_chunk_offset;
			last_chunk = &chunk_oid_lookup;
			break;

		case MIDX_OBJECT_OFFSETS_ID:
			chunk_object_offsets.offset = last_chunk_offset;
			last_chunk = &chunk_object_offsets;
			break;

		case MIDX_OBJECT_LARGE_OFFSETS_ID:
			chunk_object_large_offsets.offset = last_chunk_offset;
			last_chunk = &chunk_object_large_offsets;
			break;

		default:
			return midx_error("unrecognized chunk ID");
		}
	}
	last_chunk->length = (size_t)(trailer_offset - last_chunk_offset);

	error = midx_parse_packfile_names(
			idx, data, ntohl(hdr->packfiles), &chunk_packfile_names);
	if (error < 0)
		return error;
	error = midx_parse_oid_fanout(idx, data, &chunk_oid_fanout);
	if (error < 0)
		return error;
	error = midx_parse_oid_lookup(idx, data, &chunk_oid_lookup);
	if (error < 0)
		return error;
	error = midx_parse_object_offsets(idx, data, &chunk_object_offsets);
	if (error < 0)
		return error;
	error = midx_parse_object_large_offsets(idx, data, &chunk_object_large_offsets);
	if (error < 0)
		return error;

	return 0;
}

int git_midx_open(
		git_midx_file **idx_out,
		const char *path)
{
	git_midx_file *idx;
	git_file fd = -1;
	size_t idx_size;
	struct stat st;
	int error;

	/* TODO: properly open the file without access time using O_NOATIME */
	fd = git_futils_open_ro(path);
	if (fd < 0)
		return fd;

	if (p_fstat(fd, &st) < 0) {
		p_close(fd);
		git_error_set(GIT_ERROR_ODB, "multi-pack-index file not found - '%s'", path);
		return -1;
	}

	if (!S_ISREG(st.st_mode) || !git__is_sizet(st.st_size)) {
		p_close(fd);
		git_error_set(GIT_ERROR_ODB, "invalid pack index '%s'", path);
		return -1;
	}
	idx_size = (size_t)st.st_size;

	idx = git__calloc(1, sizeof(git_midx_file));
	GIT_ERROR_CHECK_ALLOC(idx);

	error = git_buf_sets(&idx->filename, path);
	if (error < 0)
		return error;

	error = git_futils_mmap_ro(&idx->index_map, fd, 0, idx_size);
	p_close(fd);
	if (error < 0) {
		git_midx_free(idx);
		return error;
	}

	if ((error = git_midx_parse(idx, idx->index_map.data, idx_size)) < 0) {
		git_midx_free(idx);
		return error;
	}

	*idx_out = idx;
	return 0;
}

bool git_midx_needs_refresh(
		const git_midx_file *idx,
		const char *path)
{
	git_file fd = -1;
	struct stat st;
	ssize_t bytes_read;
	git_oid idx_checksum = {{0}};

	/* TODO: properly open the file without access time using O_NOATIME */
	fd = git_futils_open_ro(path);
	if (fd < 0)
		return true;

	if (p_fstat(fd, &st) < 0) {
		p_close(fd);
		return true;
	}

	if (!S_ISREG(st.st_mode) ||
	    !git__is_sizet(st.st_size) ||
	    (size_t)st.st_size != idx->index_map.len) {
		p_close(fd);
		return true;
	}

	bytes_read = p_pread(fd, &idx_checksum, GIT_OID_RAWSZ, st.st_size - GIT_OID_RAWSZ);
	p_close(fd);

	if (bytes_read != GIT_OID_RAWSZ)
		return true;

	return !git_oid_equal(&idx_checksum, &idx->checksum);
}

int git_midx_entry_find(
		git_midx_entry *e,
		git_midx_file *idx,
		const git_oid *short_oid,
		size_t len)
{
	int pos, found = 0;
	size_t pack_index;
	uint32_t hi, lo;
	const git_oid *current = NULL;
	const unsigned char *object_offset;
	off64_t offset;

	GIT_ASSERT_ARG(idx);

	hi = ntohl(idx->oid_fanout[(int)short_oid->id[0]]);
	lo = ((short_oid->id[0] == 0x0) ? 0 : ntohl(idx->oid_fanout[(int)short_oid->id[0] - 1]));

	pos = git_pack__lookup_sha1(idx->oid_lookup, GIT_OID_RAWSZ, lo, hi, short_oid->id);

	if (pos >= 0) {
		/* An object matching exactly the oid was found */
		found = 1;
		current = idx->oid_lookup + pos;
	} else {
		/* No object was found */
		/* pos refers to the object with the "closest" oid to short_oid */
		pos = -1 - pos;
		if (pos < (int)idx->num_objects) {
			current = idx->oid_lookup + pos;

			if (!git_oid_ncmp(short_oid, current, len))
				found = 1;
		}
	}

	if (found && len != GIT_OID_HEXSZ && pos + 1 < (int)idx->num_objects) {
		/* Check for ambiguousity */
		const git_oid *next = current + 1;

		if (!git_oid_ncmp(short_oid, next, len)) {
			found = 2;
		}
	}

	if (!found)
		return git_odb__error_notfound("failed to find offset for multi-pack index entry", short_oid, len);
	if (found > 1)
		return git_odb__error_ambiguous("found multiple offsets for multi-pack index entry");

	object_offset = idx->object_offsets + pos * 8;
	offset = ntohl(*((uint32_t *)(object_offset + 4)));
	if (offset & 0x80000000) {
		uint32_t object_large_offsets_pos = offset & 0x7fffffff;
		const unsigned char *object_large_offsets_index = idx->object_large_offsets;

		/* Make sure we're not being sent out of bounds */
		if (object_large_offsets_pos >= idx->num_object_large_offsets)
			return git_odb__error_notfound("invalid index into the object large offsets table", short_oid, len);

		object_large_offsets_index += 8 * object_large_offsets_pos;

		offset = (((uint64_t)ntohl(*((uint32_t *)(object_large_offsets_index + 0)))) << 32) |
				ntohl(*((uint32_t *)(object_large_offsets_index + 4)));
	}
	pack_index = ntohl(*((uint32_t *)(object_offset + 0)));
	if (pack_index >= git_vector_length(&idx->packfile_names))
		return midx_error("invalid index into the packfile names table");
	e->pack_index = pack_index;
	e->offset = offset;
	git_oid_cpy(&e->sha1, current);
	return 0;
}

int git_midx_foreach_entry(
		git_midx_file *idx,
		git_odb_foreach_cb cb,
		void *data)
{
	size_t i;
	int error;

	GIT_ASSERT_ARG(idx);

	for (i = 0; i < idx->num_objects; ++i) {
		if ((error = cb(&idx->oid_lookup[i], data)) != 0)
			return git_error_set_after_callback(error);
	}

	return error;
}

int git_midx_close(git_midx_file *idx)
{
	GIT_ASSERT_ARG(idx);

	if (idx->index_map.data)
		git_futils_mmap_free(&idx->index_map);

	git_vector_free(&idx->packfile_names);

	return 0;
}

void git_midx_free(git_midx_file *idx)
{
	if (!idx)
		return;

	git_buf_dispose(&idx->filename);
	git_midx_close(idx);
	git__free(idx);
}
