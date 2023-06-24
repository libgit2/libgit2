/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "indexer.h"
#include "pack.h"
#include "packfile_parser.h"
#include "repository.h"
#include "sizemap.h"
#include "delta.h"

#include "git2_util.h"

#include "git2/indexer.h"

/* TODO: tweak? also align with packfile_parser.c */
#define READ_CHUNK_SIZE (1024 * 256)

size_t git_indexer__max_objects = UINT32_MAX;

struct object_entry {
	git_object_t type;
	git_object_size_t position;
	/* TODO: this can be unsigned short */
	git_object_size_t header_size;
	git_oid id;
};

struct delta_entry {
	struct object_entry object;
	git_object_t final_type;
	unsigned short chain_length;
	union {
		git_oid ref_id;
		git_object_size_t ofs_position;
	} base;
};

struct git_indexer {
	git_odb *odb;
	git_oid_t oid_type;

	/* TODO: verify! connectivity checks! */
	unsigned int do_fsync:1,
	             do_verify:1;
	unsigned int mode;

	git_indexer_progress_cb progress_cb;
	void *progress_payload;

	git_str path;
	int fd;

	git_packfile_parser parser;

	uint32_t entries;
	unsigned int started : 1,
	             complete : 1;

	/* Current object / delta being parsed */
	/* TODO: pul these directly from the parser instead? */
	git_object_size_t current_position;
	git_object_t current_type;
	git_object_size_t current_header_size;
	git_object_size_t current_size;
	git_oid current_ref; /* current ref delta base */
	git_object_size_t current_offset; /* current ofs delta base */

	git_hash_ctx hash_ctx;

	git_sizemap *positions; /* map of position to object */
	git_vector objects; /* vector of `struct object_entry` */
	git_vector deltas;  /* vector of `struct delta_entry` */

	git_oid trailer_oid;
	char name[(GIT_HASH_MAX_SIZE * 2) + 1];
};

int git_indexer_options_init(
	git_indexer_options *opts,
	unsigned int version)
{
	GIT_INIT_STRUCTURE_FROM_TEMPLATE(opts,
		version, git_indexer_options, GIT_INDEXER_OPTIONS_INIT);

	return 0;
}

static int objects_cmp(const void *a, const void *b)
{
	const struct object_entry *entry_a = a;
	const struct object_entry *entry_b = b;

	return git_oid__cmp(&entry_a->id, &entry_b->id);
}

static int parse_packfile_header(
	uint32_t version,
	uint32_t entries,
	void *data)
{
	git_indexer *indexer = (git_indexer *)data;

	GIT_UNUSED(version);

	if (indexer->started) {
		git_error_set(GIT_ERROR_INDEXER, "unexpected packfile header");
		return -1;
	}

	printf("--> header cb: %d %d\n", (int)version, (int)entries);

	/* TODO: is 50% a good number? not convinced. */
	if (git_sizemap_new(&indexer->positions) < 0 ||
	    git_vector_init(&indexer->objects, entries, objects_cmp) < 0 ||
	    git_vector_init(&indexer->deltas, entries / 2, NULL) < 0)
		return -1;

	indexer->started = 1;
	indexer->entries = entries;

	return 0;
}

static int parse_object_start(
	git_object_size_t position,
	git_object_t type,
	git_object_size_t header_size,
	git_object_size_t size,
	void *data)
{
	git_indexer *indexer = (git_indexer *)data;

	indexer->current_position = position;
	indexer->current_type = type;
	indexer->current_header_size = header_size;
	indexer->current_size = size;

	return 0;
}

static int parse_object_complete(
	git_object_size_t compressed_size,
	git_oid *oid,
	void *data)
{
	git_indexer *indexer = (git_indexer *)data;
	struct object_entry *entry;

	GIT_UNUSED(compressed_size);

	/* TODO: pool? */
	entry = git__malloc(sizeof(struct object_entry));
	GIT_ERROR_CHECK_ALLOC(entry);

	entry->type = indexer->current_type;
	git_oid_cpy(&entry->id, oid);
	entry->position = indexer->current_position;
	entry->header_size = indexer->current_header_size;

	if (git_sizemap_set(indexer->positions, entry->position, entry) < 0 ||
	    git_vector_insert(&indexer->objects, entry) < 0)
		return -1;

	return 0;
}

static int parse_delta_start(
	git_object_size_t position,
	git_object_t type,
	git_object_size_t header_size,
	git_object_size_t size,
	git_oid *delta_ref,
	git_object_size_t delta_offset,
	void *data)
{
	git_indexer *indexer = (git_indexer *)data;

	GIT_UNUSED(delta_ref);
	GIT_UNUSED(delta_offset);

	/* TODO: avoid double copy - preallocate the entry */
	indexer->current_position = position;
	indexer->current_type = type;
	indexer->current_header_size = header_size;
	indexer->current_size = size;

	if (type == GIT_OBJECT_REF_DELTA)
		git_oid_cpy(&indexer->current_ref, delta_ref);
	else
		indexer->current_offset = delta_offset;

	return 0;
}

static int parse_delta_complete(
	git_object_size_t compressed_size,
	void *data)
{
	git_indexer *indexer = (git_indexer *)data;
	struct delta_entry *entry;

	GIT_UNUSED(compressed_size);

	/* TODO: pool? */
	entry = git__malloc(sizeof(struct delta_entry));
	GIT_ERROR_CHECK_ALLOC(entry);

	entry->object.type = indexer->current_type;
	entry->object.header_size = indexer->current_header_size;
	entry->object.position = indexer->current_position;
	entry->final_type = 0;
	entry->chain_length = 0;

	if (entry->object.type == GIT_OBJECT_REF_DELTA) {
		git_oid_cpy(&entry->base.ref_id, &indexer->current_ref);
	} else {
		if (indexer->current_offset > indexer->current_position) {
			git_error_set(GIT_ERROR_INDEXER, "invalid delta offset (base would be negative)");
			return -1;
		}

		entry->base.ofs_position = indexer->current_position - indexer->current_offset;
	}

	if (git_sizemap_set(indexer->positions, entry->object.position, entry) < 0 ||
	    git_vector_insert(&indexer->objects, entry) < 0 ||
	    git_vector_insert(&indexer->deltas, entry) < 0)
		return -1;

	return 0;
}

static int parse_packfile_complete(
	const unsigned char *checksum,
	size_t checksum_len,
	void *data)
{
	git_indexer *indexer = (git_indexer *)data;

	GIT_UNUSED(checksum);
	GIT_UNUSED(checksum_len);

	indexer->complete = 1;
	return 0;
}

static int indexer_new(
	git_indexer **out,
	const char *parent_path,
	git_oid_t oid_type,
	unsigned int mode,
	git_odb *odb,
	git_indexer_options *in_opts)
{
	git_indexer_options opts = GIT_INDEXER_OPTIONS_INIT;
	git_indexer *indexer;
	git_str path = GIT_STR_INIT;
	git_hash_algorithm_t hash_type;
	int error;

	if (in_opts)
		memcpy(&opts, in_opts, sizeof(opts));

	indexer = git__calloc(1, sizeof(git_indexer));
	GIT_ERROR_CHECK_ALLOC(indexer);

	indexer->oid_type = oid_type;
	indexer->odb = odb;
	indexer->progress_cb = opts.progress_cb;
	indexer->progress_payload = opts.progress_cb_payload;
	indexer->mode = mode ? mode : GIT_PACK_FILE_MODE;
	indexer->do_verify = opts.verify;

	if (git_repository__fsync_gitdir)
		indexer->do_fsync = 1;

	hash_type = git_oid_algorithm(oid_type);

	if ((error = git_packfile_parser_init(&indexer->parser, oid_type)) < 0 ||
	    (error = git_hash_ctx_init(&indexer->hash_ctx, hash_type)) < 0 ||
	    (error = git_str_joinpath(&path, parent_path, "pack")) < 0 ||
	    (error = indexer->fd = git_futils_mktmp(&indexer->path, path.ptr, indexer->mode)) < 0)
		goto done;

	indexer->parser.packfile_header = parse_packfile_header;
	indexer->parser.object_start = parse_object_start;
	indexer->parser.object_complete = parse_object_complete;
	indexer->parser.delta_start = parse_delta_start;
	indexer->parser.delta_complete = parse_delta_complete;
	indexer->parser.packfile_complete = parse_packfile_complete;
	indexer->parser.callback_data = indexer;

done:
	git_str_dispose(&path);

	if (error < 0) {
		git__free(indexer);
		return -1;
	}

	*out = indexer;
	return 0;
}

#ifdef GIT_EXPERIMENTAL_SHA256
int git_indexer_new(
	git_indexer **out,
	const char *path,
	git_oid_t oid_type,
	git_indexer_options *opts)
{
	unsigned int mode = opts ? opts->mode : 0;
	git_odb *odb = opts ? opts->odb : NULL;

	return indexer_new(out, path, oid_type, mode, odb, opts);
}
#else
int git_indexer_new(
	git_indexer **out,
	const char *path,
	unsigned int mode,
	git_odb *odb,
	git_indexer_options *opts)
{
	return indexer_new(out, path, GIT_OID_SHA1, mode, odb, opts);
}
#endif

void git_indexer__set_fsync(git_indexer *indexer, int do_fsync)
{
	indexer->do_fsync = !!do_fsync;
}

const char *git_indexer_name(const git_indexer *indexer)
{
	return indexer->name;
}

#ifndef GIT_DEPRECATE_HARD
const git_oid *git_indexer_hash(const git_indexer *indexer)
{
	return &indexer->trailer_oid;
}
#endif

static int append_data(
	git_indexer *indexer,
	const void *data,
	size_t len)
{
	size_t chunk_len;

	while (len > 0) {
		chunk_len = min(len, SSIZE_MAX);

		if ((p_write(indexer->fd, data, chunk_len)) < 0)
			return -1;

		data += chunk_len;
		len -= chunk_len;
	}

	return 0;
}

int git_indexer_append(
	git_indexer *indexer,
	const void *data,
	size_t len,
	git_indexer_progress *stats)
{
	GIT_ASSERT_ARG(indexer && (!len || data));

	GIT_UNUSED(stats);


	/*
	 * Take two passes with the data given to us: first, actually do the
	 * appending to the packfile. Next, do whatever parsing we can.
	 */

	if (append_data(indexer, data, len) < 0)
		return -1;

	if (git_packfile_parser_parse(&indexer->parser, data, len) < 0)
		return -1;

	return 0;
}

/* TODO: this should live somewhere else -- maybe in packfile parser? */
static int unpack_raw_object(
	unsigned char **out,
	size_t *out_len,
	git_indexer *indexer,
	git_object_size_t raw_position)
{
	git_str data = GIT_STR_INIT;

	/* TODO: we know the object size, hint the git_str with it */

	/* TODO: we need to be more thoughtful about file descriptors */
	int fd = indexer->fd;


	/* TODO: assert ofs_position <= off_t */
	/* TODO: 32 bit vs 64 bit */
	if (p_lseek(fd, (off_t)raw_position, SEEK_SET) < 0) {
		git_error_set(GIT_ERROR_OS, "could not seek in packfile");
		return -1;
	}

	if (git_zstream_inflatefile(&data, fd) < 0)
		return -1;

	/* TODO: validate that data.size == expected size of this object from the positions table */

	*out_len = data.size;
	*out = (unsigned char *)git_str_detach(&data);

	return 0;
}

static int unpack_object_at_position(
	unsigned char **out,
	size_t *out_len,
	git_object_t *out_type,
	unsigned short *out_chain_length,
	git_indexer *indexer,
	git_object_size_t ofs_position)
{
	struct object_entry *object;
	git_object_size_t raw_position;
	int error;

	/*
	 * TODO: we should cache small delta bases?
	 *
	 * How often are delta bases re-used? How far back will git look for
	 * delta bases - is it constant? If so, we should call _all_ objects
	 * and expire them off the back as we continue to read them.
	 *
	 * Does git have a size limit on delta bases? Probably not, but maybe
	 * a size limit on the postimage. But we can probably assume that a
	 * delta base is roughly the same size as postimage and avoid caching
	 * those objects.
	 */

	if ((object = git_sizemap_get(indexer->positions, ofs_position)) == NULL) {
		git_error_set(GIT_ERROR_INDEXER,
			"corrupt packfile - no object at offset position %llu",
			ofs_position);
		return -1;
	}

	if (object->type == GIT_OBJECT_REF_DELTA) {
		abort();
	} else if (object->type == GIT_OBJECT_OFS_DELTA) {
		struct delta_entry *delta_entry = (struct delta_entry *)object;
		unsigned char *base, *delta;
		size_t base_len, delta_len;

		/* TODO: overflow check */
		raw_position = ofs_position + object->header_size;

		if (unpack_object_at_position(&base, &base_len, out_type, out_chain_length, indexer, delta_entry->base.ofs_position) < 0 ||
		    unpack_raw_object(&delta, &delta_len, indexer, raw_position) < 0)
			return -1;

		error = git_delta_apply((void **)out, out_len, base, base_len, delta, delta_len);
		(*out_chain_length)++;

		git__free(base);
		git__free(delta);

		return error;
	}

	/* TODO: overflow check */
	raw_position = ofs_position + object->header_size;

	if (unpack_raw_object(out, out_len, indexer, raw_position) < 0)
		return -1;

	*out_chain_length = 0;
	*out_type = object->type;

	return 0;
}

/* TODO: limit recursion depth  -- it looks like git may put a 50 length limit on delta chains? */
static int resolve_delta(git_indexer *indexer, struct delta_entry *delta)
{
	char header[64];
	unsigned char *data;
	size_t header_len, data_len;

	/* TODO: cache lookup here? */

	/* TODO: hash ctx per thread */
	if (git_hash_init(&indexer->hash_ctx) < 0)
		return -1;

	if (delta->object.type == GIT_OBJECT_REF_DELTA) {
		abort();
		return -1;
	} else {
		if (unpack_object_at_position(&data, &data_len, &delta->final_type, &delta->chain_length, indexer, delta->object.position) < 0)
			return -1;
	}

	/*
	 * TODO: we don't really need to dedeltafy the whole object just to
	 * hash it, we could hash in the dedltafication step.
	 */

	if (git_odb__format_object_header(&header_len, header, sizeof(header), data_len, delta->final_type) < 0 ||
	    git_hash_update(&indexer->hash_ctx, header, header_len) < 0 ||
		git_hash_update(&indexer->hash_ctx, data, data_len) < 0 ||
		git_hash_final(delta->object.id.id, &indexer->hash_ctx) < 0)
		return -1;

#ifdef GIT_EXPERIMENTAL_SHA256
	delta->object.id.type = indexer->oid_type;
#endif

	/*printf("resolved: %s\n", git_oid_tostr_s(&delta->object.id));*/

	return 0;
}

static int resolve_final_deltas(git_indexer *indexer)
{
	size_t deltas_len = git_vector_length(&indexer->deltas);
	struct delta_entry *delta_entry;
	size_t i;
	int cnt = 0;

	while (deltas_len > 0) {
		bool progress = false;

		printf("loop %d\n", ++cnt);

		git_vector_foreach(&indexer->deltas, i, delta_entry) {
			if (delta_entry->final_type)
				continue;

			if (resolve_delta(indexer, delta_entry) < 0)
				return -1;

			/* TODO */
			deltas_len--;

			progress = true;
		}

		if (!progress) {
			git_error_set(GIT_ERROR_INDEXER, "could not resolve deltas");
			return -1;
		}
	}

	return 0;
}

int git_indexer_commit(git_indexer *indexer, git_indexer_progress *stats)
{
	struct object_entry *entry;
	size_t i;

	GIT_ASSERT_ARG(indexer);

	GIT_UNUSED(stats);

	if (!indexer->complete) {
		git_error_set(GIT_ERROR_INDEXER, "incomplete packfile");
		return -1;
	}

	if (resolve_final_deltas(indexer) < 0)
		return -1;

	git_vector_foreach(&indexer->objects, i, entry) {
		git_object_t type = git_object__is_delta(entry->type) ?
			((struct delta_entry *)entry)->final_type : entry->type;

/*
		printf("%s %-6s ... ... %llu",
			git_oid_tostr_s(&entry->id),
			git_object_type2string(type),
			entry->position);

		if (git_object__is_delta(entry->type)) {
			printf(" %u", ((struct delta_entry *)entry)->chain_length);
		}

		printf("\n");
		*/
	}

	git_vector_sort(&indexer->objects);

	return 0;
}

void git_indexer_free(git_indexer *indexer)
{
	if (!indexer)
		return;

	git_hash_ctx_cleanup(&indexer->hash_ctx);
	git_sizemap_free(indexer->positions);
	git_vector_free(&indexer->deltas);
	git_vector_free_deep(&indexer->objects);
	git_packfile_parser_dispose(&indexer->parser);
	git__free(indexer);
}
