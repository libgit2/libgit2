/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include <sys/mman.h>

#include "indexer.h"
#include "pack.h"
#include "packfile_parser.h"
#include "repository.h"
#include "sizemap.h"
#include "delta.h"
#include "commit.h"
#include "tree.h"
#include "tag.h"

#include "git2_util.h"

#include "git2/indexer.h"

#define READ_CHUNK_SIZE (1024 * 256)

/*
 * Objects that have been seen within this amount of time will not be
 * promoted in the LRU - avoids unnecessary work for frequently seen
 * objects.
 */
#define LRU_FRESHNESS_TIME 5000

size_t git_indexer__max_objects = UINT32_MAX;

struct object_entry {
	git_object_t type;
	uint16_t header_size;
	uint32_t crc32;
	git_object_size_t position;
	git_object_size_t size;
	git_oid id;
};

struct delta_entry {
	struct object_entry object;
	git_object_t final_type;
	union {
		git_oid ref_id;
		git_object_size_t ofs_position;
	} base;
};


/*
 * object_data reflects the actual, inflated and de-deltafied object
 * data. (object_entry will store the information about the raw
 * objects themselves, which may reflect delta data not inflated data.)
 */
struct object_data {
	git_object_t type;
	size_t len;
	unsigned char data[GIT_FLEX_ARRAY];
};

struct git_indexer {
	git_odb *odb;
	git_oid_t oid_type;

	unsigned int do_fsync : 1,
	             do_verify : 1,
	             keep_thin_pack : 1;
	unsigned int mode;

	git_indexer_progress_cb progress_cb;
	void *progress_payload;

	git_str base_path;
	git_str packfile_path;
	git_str index_path;

	int packfile_fd;
	unsigned long long packfile_size;

	git_packfile_parser *parser;

	uint32_t version;
	uint32_t entries;
	unsigned int started : 1,
	             has_thin_entries : 1,
	             committed : 1;

	/* Current object / delta being parsed */
	struct object_entry *current_object;
	git_str current_object_data;
	struct delta_entry *current_delta;

	unsigned char *packfile_map;

	git_oidmap *ids; /* map of oid to object */
	git_sizemap *positions; /* map of position to object */
	git_vector objects; /* vector of `struct object_entry` */
	git_vector offset_deltas;  /* vector of `struct delta_entry` */
	git_vector ref_deltas;  /* vector of `struct delta_entry` */

	git_oidmap *expected_ids; /* object verification list */
	git_pool expected_id_pool;

	/* The packfile's trailer; and formatted into returnable objects. */
	unsigned char trailer[GIT_HASH_MAX_SIZE];
	git_oid trailer_oid;
	char trailer_name[(GIT_HASH_MAX_SIZE * 2) + 1];

	/* Current delta information */
	git_hash_ctx delta_hash_ctx;
	git_zstream delta_zstream;

	/* Statistics */
	size_t object_lookups;

	git_indexer_progress progress;

	uint64_t index_start;
	uint64_t index_end;
	uint64_t delta_start;
	uint64_t delta_end;
};


int git_indexer_options_init(
	git_indexer_options *opts,
	unsigned int version)
{
	GIT_INIT_STRUCTURE_FROM_TEMPLATE(opts,
		version, git_indexer_options, GIT_INDEXER_OPTIONS_INIT);

	return 0;
}

static int do_progress_cb(git_indexer *indexer)
{
	if (!indexer->progress_cb)
		return 0;

	return git_error_set_after_callback_function(
		indexer->progress_cb(&indexer->progress, indexer->progress_payload),
		"indexer progress");
}

static int objects_cmp(const void *a, const void *b)
{
	const struct object_entry *entry_a = a;
	const struct object_entry *entry_b = b;

	return git_oid__cmp(&entry_a->id, &entry_b->id);
}

static int offset_delta_cmp(const void *a, const void *b)
{
	const struct delta_entry *entry_a = a;
	const struct delta_entry *entry_b = b;

	if (entry_a->base.ofs_position < entry_b->base.ofs_position)
		return -1;
	else if (entry_a->base.ofs_position > entry_b->base.ofs_position)
		return 1;
	else
		return 0;
}

static int ref_delta_cmp(const void *a, const void *b)
{
	const struct delta_entry *entry_a = a;
	const struct delta_entry *entry_b = b;

	return git_oid_cmp(&entry_a->base.ref_id, &entry_b->base.ref_id);
}

static int parse_packfile_header(
	uint32_t version,
	uint32_t entries,
	void *idx)
{
	git_indexer *indexer = (git_indexer *)idx;

	if (indexer->started) {
		git_error_set(GIT_ERROR_INDEXER, "unexpected packfile header");
		return -1;
	}

	if (git_sizemap_new(&indexer->positions) < 0 ||
	    git_oidmap_new(&indexer->ids) < 0 ||
	    git_vector_init(&indexer->objects, entries, objects_cmp) < 0 ||
	    git_vector_init(&indexer->offset_deltas, entries / 2, offset_delta_cmp) < 0 ||
	    git_vector_init(&indexer->ref_deltas, entries / 2, ref_delta_cmp) < 0 ||
	    git_hash_ctx_init(&indexer->delta_hash_ctx, git_oid_algorithm(indexer->oid_type)) < 0 ||
	    git_zstream_init(&indexer->delta_zstream, GIT_ZSTREAM_INFLATE) < 0)
		return -1;

	indexer->started = 1;
	indexer->version = version;
	indexer->entries = entries;

	indexer->progress.total_objects = entries;

	return 0;
}

static int parse_object_start(
	git_object_size_t position,
	uint16_t header_size,
	git_object_t type,
	git_object_size_t size,
	void *idx)
{
	git_indexer *indexer = (git_indexer *)idx;
	struct object_entry *entry;

	entry = git__malloc(sizeof(struct object_entry));
	GIT_ERROR_CHECK_ALLOC(entry);

	entry->type = type;
	entry->position = position;
	entry->header_size = header_size;
	entry->size = size;

	indexer->current_object = entry;

	if (indexer->do_verify)
		git_str_clear(&indexer->current_object_data);

	return 0;
}

static int parse_object_data(
	void *object_data,
	size_t object_data_len,
	void *idx)
{
	git_indexer *indexer = (git_indexer *)idx;

	if (!indexer->do_verify)
		return 0;

	return git_str_put(&indexer->current_object_data,
		object_data, object_data_len);
}

static int add_expected_oid(git_indexer *indexer, const git_oid *oid)
{
	/*
	 * If we know about that object because it is stored in our ODB or
	 * because we have already processed it as part of our pack file,
	 * we do not have to expect it.
	 */

	if ((!indexer->odb || !git_odb_exists(indexer->odb, oid)) &&
	    !git_oidmap_exists(indexer->ids, oid) &&
	    !git_oidmap_exists(indexer->expected_ids, oid)) {
		git_oid *dup = git_pool_malloc(&indexer->expected_id_pool, 1);
		GIT_ERROR_CHECK_ALLOC(dup);

		git_oid_cpy(dup, oid);
		return git_oidmap_set(indexer->expected_ids, dup, dup);
	}

	return 0;
}

static int check_object_connectivity(git_indexer *indexer, git_oid *id, git_rawobj *raw)
{
	git_object *object = NULL;
	int error = -1;

	GIT_ASSERT(raw->type == GIT_OBJECT_BLOB ||
	           raw->type == GIT_OBJECT_TREE ||
	           raw->type == GIT_OBJECT_COMMIT ||
	           raw->type == GIT_OBJECT_TAG);

	if (git_oidmap_exists(indexer->expected_ids, id))
		git_oidmap_delete(indexer->expected_ids, id);

	if (git_object__from_raw(&object, raw->data, raw->len, raw->type, indexer->oid_type) < 0)
		goto done;

	/*
	 * Check whether this is a known object. If so, we can just
	 * continue as we assume that the ODB has a complete graph.
	 */
	if (indexer->odb &&
	    git_odb_exists(indexer->odb, &object->cached.oid)) {
		error = 0;
		goto done;
	}

	switch (raw->type) {
	case GIT_OBJECT_TREE: {
		git_tree *tree = (git_tree *)object;
		git_tree_entry *entry;
		size_t i;

		git_array_foreach(tree->entries, i, entry) {
			if (git_tree_entry_type(entry) == GIT_OBJECT_COMMIT)
				continue;

			if (add_expected_oid(indexer, &entry->oid) < 0)
				goto done;
		}

		break;
	}
	case GIT_OBJECT_COMMIT: {
		git_commit *commit = (git_commit *) object;
		git_oid *parent_id;
		size_t i;

		git_array_foreach(commit->parent_ids, i, parent_id) {
			if (add_expected_oid(indexer, parent_id) < 0)
				goto done;

			if (add_expected_oid(indexer, &commit->tree_id) < 0)
				goto done;
		}

		break;
	}
	case GIT_OBJECT_TAG: {
		git_tag *tag = (git_tag *) object;

		if (add_expected_oid(indexer, &tag->target) < 0)
			goto done;

		break;
	}
	case GIT_OBJECT_BLOB:
		break;
	default:
		GIT_ASSERT(!"unknown object type");
		goto done;
	}

	error = 0;

done:
	git_object_free(object);
	return error;
}

static int parse_object_complete(
	git_object_size_t compressed_size,
	uint32_t compressed_crc,
	git_oid *oid,
	void *idx)
{
	git_indexer *indexer = (git_indexer *)idx;
	struct object_entry *entry = indexer->current_object;

	GIT_UNUSED(compressed_size);

	git_oid_cpy(&entry->id, oid);
	entry->crc32 = compressed_crc;

	if (git_sizemap_set(indexer->positions, entry->position, entry) < 0 ||
	    git_oidmap_set(indexer->ids, &entry->id, entry) < 0 ||
	    git_vector_insert(&indexer->objects, entry) < 0)
		return -1;

	if (indexer->do_verify) {
		git_rawobj raw = {
			indexer->current_object_data.ptr,
			indexer->current_object_data.size,
			indexer->current_object->type
		};

		if (check_object_connectivity(indexer, &entry->id, &raw) < 0)
			return -1;
	}

	indexer->current_object = NULL;

	indexer->progress.received_objects++;
	indexer->progress.indexed_objects++;

	return do_progress_cb(indexer);
}

static int parse_delta_start(
	git_object_size_t position,
	git_object_t type,
	uint16_t header_size,
	git_object_size_t size,
	git_oid *delta_ref,
	git_object_size_t delta_offset,
	void *idx)
{
	git_indexer *indexer = (git_indexer *)idx;
	struct delta_entry *entry;

	entry = git__malloc(sizeof(struct delta_entry));
	GIT_ERROR_CHECK_ALLOC(entry);

	entry->object.position = position;
	entry->object.type = type;
	entry->object.header_size = header_size;
	entry->object.size = size;

	if (type == GIT_OBJECT_REF_DELTA) {
		git_oid_cpy(&entry->base.ref_id, delta_ref);
	} else {
		if (delta_offset > position) {
			git_error_set(GIT_ERROR_INDEXER, "invalid delta offset (base would be negative)");
			return -1;
		}

		entry->base.ofs_position = position - delta_offset;
	}

	indexer->current_delta = entry;

	return 0;
}

static int parse_delta_data(
	void *delta_data,
	size_t delta_data_len,
	void *idx)
{
	git_indexer *indexer = (git_indexer *)idx;

	GIT_UNUSED(delta_data);
	GIT_UNUSED(delta_data_len);
	GIT_UNUSED(indexer);

	return 0;
}

static int parse_delta_complete(
	git_object_size_t compressed_size,
	uint32_t compressed_crc,
	void *idx)
{
	git_indexer *indexer = (git_indexer *)idx;
	struct delta_entry *entry = indexer->current_delta;
	git_vector *delta_vec;

	GIT_UNUSED(compressed_size);

	entry->object.crc32 = compressed_crc;
	entry->final_type = 0;

	if (entry->object.type == GIT_OBJECT_OFS_DELTA)
		delta_vec = &indexer->offset_deltas;
	else if (entry->object.type == GIT_OBJECT_REF_DELTA)
		delta_vec = &indexer->ref_deltas;
	else
		GIT_ASSERT(!"invalid delta type");

	if (git_sizemap_set(indexer->positions, entry->object.position, entry) < 0 ||
	    git_vector_insert(&indexer->objects, entry) < 0 ||
	    git_vector_insert(delta_vec, entry) < 0)
		return -1;

	indexer->current_delta = NULL;
	indexer->progress.received_objects++;

	return do_progress_cb(indexer);
}

static int parse_packfile_complete(
	const unsigned char *checksum,
	size_t checksum_len,
	void *idx)
{
	git_indexer *indexer = (git_indexer *)idx;

	GIT_ASSERT(checksum_len == git_oid_size(indexer->oid_type));

	memcpy(indexer->trailer, checksum, checksum_len);

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
	git_packfile_parser_options parser_opts = {0};
	git_indexer *indexer;
	int error;

	GIT_ASSERT_ARG(out);
	GIT_ASSERT_ARG(parent_path);

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

	parser_opts.packfile_header = parse_packfile_header;
	parser_opts.object_start = parse_object_start;
	parser_opts.object_data = parse_object_data;
	parser_opts.object_complete = parse_object_complete;
	parser_opts.delta_start = parse_delta_start;
	parser_opts.delta_data = parse_delta_data;
	parser_opts.delta_complete = parse_delta_complete;
	parser_opts.packfile_complete = parse_packfile_complete;
	parser_opts.callback_data = indexer;

	if (git_repository__fsync_gitdir)
		indexer->do_fsync = 1;

	if ((error = git_packfile_parser_new(&indexer->parser, oid_type, &parser_opts)) < 0 ||
	    (error = git_str_joinpath(&indexer->base_path, parent_path, "pack")) < 0 ||
	    (error = indexer->packfile_fd = git_futils_mktmp(&indexer->packfile_path,
			indexer->base_path.ptr, indexer->mode)) < 0)
		goto done;

	if (indexer->do_verify) {
		if (git_pool_init(&indexer->expected_id_pool, sizeof(git_oid)) < 0 ||
		    git_oidmap_new(&indexer->expected_ids) < 0)
			goto done;
	}

done:
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
	return indexer->trailer_name;
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
	size_t len,
	bool local_object)
{
	size_t chunk_len;

	/*
	 * TODO: make sure that we adjust the mmap after appending data?
	 * or prove that's not truly necessary
	 */

	while (len > 0) {
		chunk_len = min(len, SSIZE_MAX);

		if ((p_write(indexer->packfile_fd, data, chunk_len)) < 0)
			return -1;

		data += chunk_len;
		len -= chunk_len;

		indexer->packfile_size += chunk_len;

		/*
		 * TODO: instead of updated `received_bytes` in the
		 * indexer, should we be updating this in fetch?
		 */
		if (!local_object)
			indexer->progress.received_bytes += chunk_len;
	}

	return 0;
}

int git_indexer_append(
	git_indexer *indexer,
	const void *data,
	size_t len,
	git_indexer_progress *stats)
{
	int error;

	GIT_ASSERT_ARG(indexer && (!len || data));

	if (!indexer->index_start)
		indexer->index_start = git_time_monotonic();


	/*
	 * Take two passes with the data given to us: first, actually do the
	 * appending to the packfile. Next, do whatever parsing we can.
	 */

	if (append_data(indexer, data, len, false) < 0)
		return -1;

	if ((error = git_packfile_parser_parse(indexer->parser, data, len)) != 0)
		return error;

	if (stats)
		memcpy(stats, &indexer->progress, sizeof(git_indexer_progress));

	return 0;
}

static int load_raw_object(
	struct object_data **out,
	git_indexer *indexer,
	struct object_entry *object)
{
	struct object_data *data = NULL;
	size_t data_remain, data_size, raw_position;
	unsigned char *compressed_ptr, *data_ptr;

	GIT_ERROR_CHECK_ALLOC_ADD(&raw_position, object->position, object->header_size);
	GIT_ERROR_CHECK_ALLOC_ADD(&data_size, sizeof(struct object_data) + 1, object->size);

	data = git__calloc(1, data_size);
	GIT_ERROR_CHECK_ALLOC(data);

	data->len = object->size;
	data->type = object->type;

	data_ptr = data->data;
	data_remain = object->size;

	compressed_ptr = indexer->packfile_map + raw_position;

	git_zstream_reset(&indexer->delta_zstream);

	if (git_zstream_set_input(&indexer->delta_zstream, compressed_ptr,
			(indexer->packfile_size - raw_position)) < 0)
		goto on_error;

	while (data_remain && !git_zstream_eos(&indexer->delta_zstream)) {
		size_t data_written = data_remain;

	    if (git_zstream_get_output(data_ptr, &data_written, &indexer->delta_zstream) < 0)
			goto on_error;

		data_ptr += data_remain;
		data_remain -= data_written;
	}

	if (data_remain > 0 || !git_zstream_eos(&indexer->delta_zstream)) {
		git_error_set(GIT_ERROR_INDEXER, "object data did not match expected size");
		goto on_error;
	}

	*out = data;
	return 0;

on_error:
	git__free(data);
	return -1;
}

static int load_resolved_object(
	struct object_data **out,
	git_indexer *indexer,
	struct object_entry *object,
	struct object_entry *base);

GIT_INLINE(git_hash_algorithm_t) indexer_hash_algorithm(git_indexer *indexer)
{
	switch (indexer->oid_type) {
		case GIT_OID_SHA1:
			return GIT_HASH_ALGORITHM_SHA1;
#ifdef GIT_EXPERIMENTAL_SHA256
		case GIT_OID_SHA256:
			return GIT_HASH_ALGORITHM_SHA256;
#endif
	}

	return GIT_HASH_ALGORITHM_NONE;
}

/*
 * TODO: we should have a mechanism for inserting objects directly from
 * an existing pack. keep this as a fallback for loose odb.
 */
static int insert_thin_base(
	struct object_entry **out,
	git_indexer *indexer,
	git_oid *base_id)
{
	size_t checksum_size;
	git_odb_object *base;
	unsigned char header[GIT_OBJECT_HEADER_MAX_LEN];
	const void *base_data;
	size_t base_position, base_len, header_len;
	git_str deflate_buf = GIT_STR_INIT;
	struct object_entry *entry;
	git_object_t base_type;
	uint32_t base_crc;
	int error;

	checksum_size = git_hash_size(indexer_hash_algorithm(indexer));

	if (!indexer->odb)
		return GIT_ENOTFOUND;

	if ((error = git_odb_read(&base, indexer->odb, base_id)) < 0)
		return -1;

	/*
	 * If this is our first thin base, rewind back in the packfile over
	 * the packfile trailer so that we can insert new objects. The new
	 * trailer will be written in finalize_thin_pack.
	 */
	if (!indexer->has_thin_entries) {
		GIT_ASSERT(indexer->packfile_size > checksum_size);
		indexer->packfile_size -= checksum_size;

		if (p_lseek(indexer->packfile_fd, indexer->packfile_size, SEEK_SET) < 0) {
			git_error_set(GIT_ERROR_OS, "could not rewind packfile to fix thin pack");
			return -1;
		}
	}

	base_position = indexer->packfile_size;
	base_data = git_odb_object_data(base);
	base_len = git_odb_object_size(base);
	base_type = git_odb_object_type(base);

	if (git_packfile__object_header(&header_len, header, base_len, base_type) < 0 ||
	    git_zstream_deflatebuf(&deflate_buf, base_data, base_len) < 0 ||
	    append_data(indexer, header, header_len, true) < 0 ||
	    append_data(indexer, deflate_buf.ptr, deflate_buf.size, true) < 0) {
		error = -1;
		goto done;
	}

	base_crc = crc32(0L, Z_NULL, 0);
	base_crc = crc32(base_crc, header, header_len);
	base_crc = crc32(base_crc, (const unsigned char *)deflate_buf.ptr, deflate_buf.size);

	/* Add this to our record-keeping */

	entry = git__malloc(sizeof(struct object_entry));
	GIT_ERROR_CHECK_ALLOC(entry);

	GIT_ASSERT(header_len <= UINT16_MAX);

	entry->type = base_type;
	entry->position = base_position;
	entry->header_size = header_len;
	entry->size = base_len;

	git_oid_cpy(&entry->id, base_id);
	entry->crc32 = base_crc;

	if (git_sizemap_set(indexer->positions, entry->position, entry) < 0 ||
	    git_oidmap_set(indexer->ids, &entry->id, entry) < 0 ||
	    git_vector_insert(&indexer->objects, entry) < 0)
		return -1;

	indexer->progress.local_objects++;
	indexer->has_thin_entries = 1;

	*out = entry;
	error = 0;

done:
	git_str_dispose(&deflate_buf);
	git_odb_object_free(base);
	return error;
}

GIT_INLINE(int) load_resolved_delta_object(
	struct object_data **out,
	git_indexer *indexer,
	struct object_entry *_delta,
	struct object_entry *base)
{
	struct delta_entry *delta = (struct delta_entry *)_delta;
	struct object_data *base_data, *delta_data, *result_data;
	size_t base_size, result_size, result_data_size;
	int error;

	/* load the base */
	if (!base && delta->object.type == GIT_OBJECT_OFS_DELTA) {
		base = git_sizemap_get(indexer->positions, delta->base.ofs_position);

		if (!base) {
			git_error_set(GIT_ERROR_INDEXER,
				"corrupt packfile - no object at offset position %llu",
				(unsigned long long)delta->base.ofs_position);
			return -1;
		}
	} else if (!base && delta->object.type == GIT_OBJECT_REF_DELTA) {
		base = git_oidmap_get(indexer->ids, &delta->base.ref_id);

		if (!base && !indexer->keep_thin_pack &&
		    (error = insert_thin_base(&base, indexer, &delta->base.ref_id)) < 0)
			return error;

		if (!base) {
			git_error_set(GIT_ERROR_INDEXER,
				"corrupt packfile - no object id %s",
				git_oid_tostr_s(&delta->base.ref_id));
			return -1;
		}
	}

	GIT_ASSERT(base);

	if (load_resolved_object(&base_data, indexer, base, NULL) < 0 ||
	    load_raw_object(&delta_data, indexer, _delta) < 0 ||
		git_delta_read_header(&base_size, &result_size,
			delta_data->data, delta_data->len) < 0)
		return -1;

	GIT_ERROR_CHECK_ALLOC_ADD(&result_data_size,
		sizeof(struct object_data) + 1, result_size);

	result_data = git__calloc(1, result_data_size);
	result_data->data[result_size] = '\0';
	result_data->len = result_size;
	result_data->type = base_data->type;

	if (git_delta_apply_to_buf(result_data->data, result_data->len,
			base_data->data, base_data->len,
			delta_data->data, delta_data->len) < 0) {
		git__free(result_data);
		return -1;
	}

	git__free(base_data);
	git__free(delta_data);

	*out = result_data;

	return 0;
}

static int load_resolved_object(
	struct object_data **out,
	git_indexer *indexer,
	struct object_entry *object,
	struct object_entry *base)
{
	struct object_data *data;
	int error;

	indexer->object_lookups++;

	if (object->type == GIT_OBJECT_REF_DELTA || object->type == GIT_OBJECT_OFS_DELTA) {
		error = load_resolved_delta_object(&data, indexer, object, base);

		if (error < 0)
			return error;
	} else {
		if (load_raw_object(&data, indexer, object) < 0)
			return -1;
	}

	*out = data;
	return 0;
}

GIT_INLINE(int) resolve_delta(
	git_indexer *indexer,
	struct delta_entry *delta,
	struct object_entry *base)
{
	struct object_data *result;
	char header[64];
	size_t header_len;
	int error;

	if ((error = load_resolved_object(&result, indexer, (struct object_entry *)delta, base)) < 0)
		return error;

	if (git_hash_init(&indexer->delta_hash_ctx) < 0 ||
	    git_odb__format_object_header(&header_len, header, sizeof(header), result->len, result->type) < 0 ||
	    git_hash_update(&indexer->delta_hash_ctx, header, header_len) < 0 ||
	    git_hash_update(&indexer->delta_hash_ctx, result->data, result->len) < 0 ||
	    git_hash_final(delta->object.id.id, &indexer->delta_hash_ctx) < 0) {
		error = -1;
		goto done;
	}

#ifdef GIT_EXPERIMENTAL_SHA256
	delta->object.id.type = indexer->oid_type;
#endif

	delta->final_type = result->type;

	if ((error = git_oidmap_set(indexer->ids, &delta->object.id, &delta->object)) < 0)
		goto done;

	if (indexer->do_verify) {
		git_rawobj raw = { result->data, result->len, result->type };

		if (check_object_connectivity(indexer, &delta->object.id, &raw) < 0) {
			error = -1;
			goto done;
		}
	}

	indexer->progress.indexed_deltas++;
	indexer->progress.indexed_objects++;

	error = do_progress_cb(indexer);

done:
	git__free(result);
	return error;
}

GIT_INLINE(int) hash_and_write(
	FILE *fp,
	git_hash_ctx *hash_ctx,
	const void *data,
	size_t len)
{
	if (fwrite(data, 1, len, fp) < len ||
	    git_hash_update(hash_ctx, data, len) < 0)
		return -1;

	return 0;
}

static int write_index(git_indexer *indexer)
{
	git_hash_algorithm_t hash_type;
	git_hash_ctx hash_ctx;
	struct object_entry *entry;
	uint8_t fanout = 0;
	uint32_t fanout_count = 0, nl, long_offset = 0;
	uint64_t nll;
	size_t oid_size, i = 0;
	unsigned char index_trailer[GIT_HASH_MAX_SIZE];
	int fd = -1;
	FILE *fp = NULL;

	hash_type = git_oid_algorithm(indexer->oid_type);

	GIT_ASSERT(git_vector_length(&indexer->objects) <= UINT32_MAX);

	if (git_hash_ctx_init(&hash_ctx, hash_type) < 0 ||
	    git_hash_init(&hash_ctx) < 0 ||
	    git_str_join(&indexer->index_path, '.', indexer->packfile_path.ptr, "idx") < 0 ||
	    (fd = p_open(indexer->index_path.ptr, O_RDWR|O_CREAT, indexer->mode)) < 0 ||
		(fp = fdopen(fd, "w")) == NULL)
		goto on_error;

	if (hash_and_write(fp, &hash_ctx, "\377tOc\000\000\000\002", 8) < 0)
		goto on_error;

	/* Write fanout section */
	do {
		while (i < git_vector_length(&indexer->objects) &&
		       (entry = indexer->objects.contents[i]) &&
			   entry->id.id[0] == fanout) {
			fanout_count++;
			i++;
		}

		nl = htonl(fanout_count);

		if (hash_and_write(fp, &hash_ctx, &nl, 4) < 0)
			goto on_error;
	} while (fanout++ < 0xff);

	/* Write object IDs */
	oid_size = git_oid_size(indexer->oid_type);
	git_vector_foreach(&indexer->objects, i, entry) {
		if (hash_and_write(fp, &hash_ctx, entry->id.id, oid_size) < 0)
			goto on_error;
	}

	/* Write the CRC32s */
	git_vector_foreach(&indexer->objects, i, entry) {
		nl = htonl(entry->crc32);

		if (hash_and_write(fp, &hash_ctx, &nl, sizeof(uint32_t)) < 0)
			goto on_error;
	}

	/* Small (31-bit) offsets */
	git_vector_foreach(&indexer->objects, i, entry) {
		if (entry->position > 0x7fffffff) {
			nl = htonl(0x80000000 | long_offset++);
		} else {
			nl = htonl(entry->position);
		}

		if (hash_and_write(fp, &hash_ctx, &nl, sizeof(uint32_t)) < 0)
			goto on_error;
	}

	/* Long (>31-bit) offsets */
	if (long_offset > 0) {
		git_vector_foreach(&indexer->objects, i, entry) {
			if (entry->position > 0x7fffffff) {
				nll = htonll(entry->position);

				if (hash_and_write(fp, &hash_ctx, &nll, sizeof(uint64_t)) < 0)
					goto on_error;
			}
		}
	}

	if (hash_and_write(fp, &hash_ctx, indexer->trailer, git_oid_size(indexer->oid_type)) < 0)
		goto on_error;

	if (git_hash_final(index_trailer, &hash_ctx) < 0 ||
	    fwrite(index_trailer, 1, git_oid_size(indexer->oid_type), fp) < git_oid_size(indexer->oid_type))
		goto on_error;

	if (indexer->do_fsync && p_fsync(fd) < 0) {
		git_error_set(GIT_ERROR_OS, "failed to fsync packfile index");
		goto on_error;
	}

	/* fclose will close the underlying fd */
	fclose(fp); fp = NULL; fd = -1;

	return 0;

on_error:
	if (fp != NULL)
		fclose(fp);
	else if (fd != -1)
		p_close(fd);

	if (indexer->index_path.size > 0)
		p_unlink(indexer->index_path.ptr);

	git_hash_ctx_cleanup(&hash_ctx);
	return -1;
}

/* Resolve deltas. Each thread will do a percentage of the resolution. */
static int resolve_offset_deltas(git_indexer *indexer)
{
	struct object_entry *object_entry;
	struct delta_entry *delta_start, *delta_entry;
	size_t deltas_cnt, object_idx, delta_idx = 0, end_idx;
	int error;

	if ((deltas_cnt = git_vector_length(&indexer->offset_deltas)) == 0)
		return 0;

	git_vector_sort(&indexer->offset_deltas);

	delta_start = git_vector_get(&indexer->offset_deltas, delta_idx);
	end_idx = git_vector_length(&indexer->offset_deltas);

	/*
	 * At this point, our deltas are sorted by the positions of their
	 * bases. Loop over the objects and applying all the deltas that
	 * use this object as their base.
	 */
	git_vector_foreach(&indexer->objects, object_idx, object_entry) {
		/* We may not have gotten to this delta yet */
		if (object_entry->position < delta_start->base.ofs_position)
			continue;

		while (delta_idx < end_idx) {
			delta_entry = git_vector_get(&indexer->offset_deltas, delta_idx);
			GIT_ASSERT(delta_entry->object.type == GIT_OBJECT_OFS_DELTA);

			/*
			 * We've hit the end of the deltas that use this object
			 * as a base.
			 */
			if (delta_entry->base.ofs_position > object_entry->position)
				break;

			if ((error = resolve_delta(indexer, delta_entry, object_entry)) < 0)
				return error;

			delta_idx++;
		}
	}

	return do_progress_cb(indexer);
}

static int resolve_ref_deltas(git_indexer *indexer)
{
	struct delta_entry *delta_entry;
	size_t delta_idx;
	bool progress = false;
	int error;

	do {
		progress = false;

		git_vector_foreach(&indexer->ref_deltas, delta_idx, delta_entry) {
			/* Skip this if we've already resolved this. */
			if (delta_entry->final_type)
				continue;

			error = resolve_delta(indexer, delta_entry, NULL);

			if (error == GIT_ENOTFOUND)
				continue;
			else if (error < 0)
				return error;

			progress = true;
		}
	} while (progress);

	return 0;
}

static int finalize_thin_pack(git_indexer *indexer)
{
	git_hash_algorithm_t hash_type;
	git_hash_ctx hash_ctx;
	struct git_pack_header header;
	struct delta_entry *delta_entry;
	size_t delta_idx, content_size;
	char buf[READ_CHUNK_SIZE];
	int ret;

	/* Ensure that we've resolved all the deltas. */
	git_vector_foreach(&indexer->ref_deltas, delta_idx, delta_entry) {
		if (delta_entry->final_type)
			continue;

		git_error_set(GIT_ERROR_INDEXER,
			"could not find base object '%s' to resolve delta",
			git_oid_tostr_s(&delta_entry->base.ref_id));
		return -1;
	}

	/* Update the header to include the number of injected objects. */
	if (!indexer->has_thin_entries)
		return 0;

	GIT_ASSERT(indexer->progress.local_objects <= UINT32_MAX - indexer->entries);

	hash_type = git_oid_algorithm(indexer->oid_type);

	if (git_hash_ctx_init(&hash_ctx, hash_type) < 0)
		return -1;

	header.hdr_signature = htonl(PACK_SIGNATURE);
	header.hdr_version = htonl(indexer->version);
	header.hdr_entries = htonl(indexer->entries + indexer->progress.local_objects);

	/* Write the updated header and rehash the packfile. */

	if (p_lseek(indexer->packfile_fd, 0, SEEK_SET) < 0) {
		git_error_set(GIT_ERROR_OS, "could not rewind packfile to update header");
		return -1;
	}

	if (p_write(indexer->packfile_fd, &header, sizeof(struct git_pack_header)) < 0) {
		git_error_set(GIT_ERROR_OS, "could not write updated packfile header");
		return -1;
	}

	if (git_hash_update(&hash_ctx, &header, sizeof(struct git_pack_header)) < 0)
		return -1;

	GIT_ASSERT(indexer->packfile_size >= sizeof(struct git_pack_header));
	content_size = indexer->packfile_size - sizeof(struct git_pack_header);

	while (content_size > 0) {
		ret = p_read(indexer->packfile_fd, buf, min(content_size, sizeof(buf)));

		if (ret == 0) {
			break;
		} else if (ret < 0) {
			git_error_set(GIT_ERROR_OS, "could not read packfile to rehash");
			return -1;
		}

		if (git_hash_update(&hash_ctx, buf, ret) < 0)
			return -1;

		content_size -= ret;
	}

	if (git_hash_final(indexer->trailer, &hash_ctx) < 0) {
		git_error_set(GIT_ERROR_OS, "could not rehash packfile");
		return -1;
	}

	return append_data(indexer, indexer->trailer, git_hash_size(hash_type), true);
}

int git_indexer_commit(git_indexer *indexer, git_indexer_progress *stats)
{
	git_str packfile_path = GIT_STR_INIT, index_path = GIT_STR_INIT;
	int error;

	GIT_ASSERT_ARG(indexer);

	if (!git_packfile_parser_complete(indexer->parser)) {
		git_error_set(GIT_ERROR_INDEXER, "incomplete packfile");
		goto on_error;
	}

	/* Freeze the number of deltas */
	indexer->progress.total_deltas =
		indexer->progress.total_objects - indexer->progress.indexed_objects;

	if (stats)
		memcpy(stats, &indexer->progress, sizeof(git_indexer_progress));

	if ((error = do_progress_cb(indexer)) != 0)
		return error;

	indexer->index_end = git_time_monotonic();

	/* TODO: optionally don't mmap / seek instead */
	indexer->packfile_map = mmap(NULL, indexer->packfile_size,
		PROT_READ, MAP_SHARED, indexer->packfile_fd, 0);

	if (resolve_offset_deltas(indexer) < 0 ||
	    resolve_ref_deltas(indexer) < 0 ||
	    finalize_thin_pack(indexer) < 0)
		goto on_error;

	error = munmap(indexer->packfile_map, indexer->packfile_size);
	indexer->packfile_map = NULL;

	if (error)
		goto on_error;

	if (indexer->do_verify) {
		size_t missing = git_oidmap_size(indexer->expected_ids);

		if (missing > 0) {
			git_error_set(GIT_ERROR_INDEXER,
				"packfile is missing %" PRIuZ " object%s",
				missing, missing != 1 ? "s" : "");
			error = -1;
			goto on_error;
		}
	}

	if (git_oid__fromraw(&indexer->trailer_oid, indexer->trailer, indexer->oid_type) < 0 ||
	    git_hash_fmt(indexer->trailer_name, indexer->trailer, git_oid_size(indexer->oid_type)) < 0)
		goto on_error;

	if (indexer->do_fsync && p_fsync(indexer->packfile_fd) < 0) {
		git_error_set(GIT_ERROR_OS, "failed to fsync packfile");
		goto on_error;
	}

	p_close(indexer->packfile_fd);
	indexer->packfile_fd = -1;

	git_vector_sort(&indexer->objects);

	if (write_index(indexer) < 0)
		goto on_error;



	/* Move the temp packfile and index to their final location */

	if (git_str_puts(&packfile_path, indexer->base_path.ptr) < 0 ||
	    git_str_putc(&packfile_path, '-') < 0 ||
	    git_str_puts(&packfile_path, indexer->trailer_name) < 0 ||
	    git_str_puts(&index_path, packfile_path.ptr) < 0 ||
	    git_str_puts(&packfile_path, ".pack") < 0 ||
	    git_str_puts(&index_path, ".idx") < 0)
		goto on_error;

	if (p_rename(indexer->packfile_path.ptr, packfile_path.ptr) < 0 ||
	    p_rename(indexer->index_path.ptr, index_path.ptr) < 0)
		goto on_error;

	if (indexer->do_fsync &&
	    git_futils_fsync_parent(indexer->packfile_path.ptr) < 0)
		goto on_error;

	if (stats)
		memcpy(stats, &indexer->progress, sizeof(git_indexer_progress));

	git_str_dispose(&packfile_path);
	git_str_dispose(&index_path);
	return 0;

on_error:
	git_str_dispose(&packfile_path);
	git_str_dispose(&index_path);
	return -1;
}

void git_indexer_free(git_indexer *indexer)
{
	if (!indexer)
		return;

	if (indexer->packfile_fd != -1)
		p_close(indexer->packfile_fd);

	if (indexer->packfile_fd != -1 && !indexer->committed)
		p_unlink(indexer->packfile_path.ptr);

	if (indexer->do_verify) {
		git_oidmap_free(indexer->expected_ids);
		git_pool_clear(&indexer->expected_id_pool);
	}

	if (indexer->current_delta)
		git__free(indexer->current_delta);

	git_hash_ctx_cleanup(&indexer->delta_hash_ctx);
	git_zstream_free(&indexer->delta_zstream);
	git_str_dispose(&indexer->current_object_data);
	git_str_dispose(&indexer->index_path);
	git_str_dispose(&indexer->packfile_path);
	git_str_dispose(&indexer->base_path);
	git_sizemap_free(indexer->positions);
	git_oidmap_free(indexer->ids);
	git_vector_free(&indexer->offset_deltas);
	git_vector_free(&indexer->ref_deltas);
	git_vector_free_deep(&indexer->objects);
	git_packfile_parser_free(indexer->parser);
	git__free(indexer);
}
