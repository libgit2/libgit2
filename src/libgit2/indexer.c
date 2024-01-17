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

#include "git2_util.h"

#include "git2/indexer.h"

/* TODO: tweak? also align with packfile_parser.c */
#define READ_CHUNK_SIZE (1024 * 256)

#define THREADS_MAX 64
#define THREADS_DEFAULT 1

size_t git_indexer__max_objects = UINT32_MAX;

struct object_entry {
	git_object_t type;
	git_object_size_t position;
	/* TODO: this can be unsigned short */
	git_object_size_t header_size;
	git_object_size_t size;
	uint32_t crc32;
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


struct object_cache_entry {
	struct object_cache_entry *prev;
	struct object_cache_entry *next;
	uint64_t time;
	git_refcount rc;
	git_object_size_t position;
};

/*
 * object_data reflects the actual, inflated and de-deltafied object
 * data. (object_entry will store the information about the raw
 * objects themselves, which may reflect delta data not inflated data.)
 */
struct object_data {
	struct object_cache_entry cache_entry;

	/* TODO: can we shrink this to fit both in 64 bits */
	size_t len;
	git_object_t type;
	unsigned char data[GIT_FLEX_ARRAY];
};

struct object_cache {
	git_mutex lock;
	git_sizemap *map;
	size_t size;
	size_t used;
	struct object_cache_entry *oldest;
	struct object_cache_entry *newest;
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

	git_str base_path;
	git_str packfile_path;
	git_str index_path;

	int packfile_fd;
	unsigned long long packfile_size;

	git_packfile_parser parser;

	uint32_t version;
	uint32_t entries;
	unsigned int started : 1,
	             complete : 1,
				 has_thin_entries : 1,
				 committed : 1;

	/* Current object / delta being parsed */
	/* TODO: pul these directly from the parser instead? */
	git_object_size_t current_position;
	git_object_t current_type;
	git_object_size_t current_header_size;
	git_object_size_t current_size;
	git_oid current_ref; /* current ref delta base */
	git_object_size_t current_offset; /* current ofs delta base */

	struct object_entry *current_object;
	struct delta_entry *current_delta;

	unsigned char *packfile_map;

	git_oidmap *ids; /* map of oid to object */
	git_sizemap *positions; /* map of position to object */
	git_vector objects; /* vector of `struct object_entry` */
	git_vector offset_deltas;  /* vector of `struct delta_entry` */
	git_vector ref_deltas;  /* vector of `struct delta_entry` */

	struct object_cache basecache; /* lru of position to entry data */

	/* The packfile's trailer; and formatted into returnable objects. */
	unsigned char trailer[GIT_HASH_MAX_SIZE];
	git_oid trailer_oid;
	char trailer_name[(GIT_HASH_MAX_SIZE * 2) + 1];

	size_t object_lookups;
	size_t cache_hits;

	git_indexer_progress progress;

	uint64_t index_start;
	uint64_t index_end;
	uint64_t delta_start;
	uint64_t delta_end;
};

struct resolver_context {
	git_indexer *indexer;
	git_hash_ctx hash_ctx;
	git_zstream zstream;
	int send_progress_updates : 1,
	    fix_thin_packs : 1;
};

struct offset_resolver_context {
	struct resolver_context base;

	size_t thread_number;
	size_t start_idx;
	size_t end_idx;
};


static int object_cache_init(struct object_cache *cache) {
	memset(cache, 0, sizeof(struct object_cache));

	if (git_sizemap_new(&cache->map) < 0 ||
	    git_mutex_init(&cache->lock) < 0)
		return -1;

	/* TODO */
	cache->size = (1024 * 1024 * 256);

	return 0;
}

void dump_cache(struct object_cache *cache)
{
	struct object_cache_entry *entry = cache->oldest;
	uint64_t tots = 0;

	printf("cache: oldest");

	while (entry != NULL) {
		struct object_data *od = (struct object_data *)entry;

		printf(" -> %llu [%p, %lu, %d]", entry->position, entry, ((struct object_data *)entry)->len, GIT_REFCOUNT_VAL(entry));
		entry = entry->next;

		if (entry->next)
			assert(entry->next->prev == entry);

		if (entry->prev)
			assert(entry->prev->next == entry);

		tots += od->len;
	}

	printf (" -> newest\n");

	if (tots != cache->used)
		abort();
}

static struct object_data *object_cache_get(
	struct object_cache *cache,
	git_object_size_t position)
{
	struct object_cache_entry *entry;
	uint64_t last_update_time;

	if (git_mutex_lock(&cache->lock) < 0)
		return NULL;

	entry = git_sizemap_get(cache->map, position);

	if (!entry)
		goto done;

	/* Increase refcount before returning; user will decrease. */
	GIT_REFCOUNT_INC(entry);

	/* Update the timestamp */
	last_update_time = entry->time;
	entry->time = git_time_monotonic();

	/* TODO */
	if (entry->time - last_update_time < 5000 ||
	    entry == cache->newest)
		goto done;

	/* Remove this entry from its current position */
	if (entry->prev)
		entry->prev->next = entry->next;
	if (entry->next)
		entry->next->prev = entry->prev;

	/* Put this at the beginning of the list if it's the only item */
	if (cache->oldest == entry && entry->next)
		cache->oldest = entry->next;

	/* Update this entry with its next/prev pointers */
	entry->prev = cache->newest;
	entry->next = NULL;

	/* Update the newest entry item */
	cache->newest->next = entry;
	cache->newest = entry;

done:
	git_mutex_unlock(&cache->lock);
	return (struct object_data *)entry;
}

static void object_cache_free(void *data)
{
	git__free(data);
}

GIT_INLINE(int) object_cache_reserve(
	struct object_cache *cache,
	git_object_size_t size)
{
	struct object_cache_entry *old;
	struct object_data *old_data;

/*	dump_cache(cache); */

	while (cache->oldest && (cache->size - cache->used) < size) {
		old = cache->oldest;
		old_data = (struct object_data *)old;

		cache->oldest = old->next;

		GIT_ASSERT(cache->used >= old_data->len);
		GIT_ASSERT(!old->prev);

		if (old->next)
			old->next->prev = NULL;

		if (cache->newest == old)
			cache->newest = NULL;

		git_sizemap_delete(cache->map, old->position);

		cache->used -= old_data->len;

		/*
		 * Decrease the refcount since we're removing this from the LRU.
		 * Callers may still have a reference to this, but will decrease
		 * the refcount on their own.
		 */
		GIT_REFCOUNT_DEC(old, object_cache_free);
	}

	return 0;
}

/* TODO: object cache should also take an oid so ref deltas can use it */
static int object_cache_put(
	struct object_cache *cache,
	git_object_size_t position,
	struct object_data *data)
{
	struct object_cache_entry *entry = (struct object_cache_entry *)data;
	int error = 0;

	if (git_mutex_lock(&cache->lock) < 0)
		return -1;

	GIT_ASSERT_WITH_CLEANUP(cache->used <= cache->size, {
		error = -1;
		goto done;
	});

	/* TODO: cache size limits */
	if (data->len > cache->size)
		goto done;

	/* hmm, thread contentionp here? */
/*	GIT_ASSERT(git_sizemap_get(cache->map, position) == NULL); */
	GIT_ASSERT(entry->prev == NULL);
	GIT_ASSERT(entry->next == NULL);

	/* TODO: reserve needs to return when it cannot make space for
	 * this object, and we should not cache it.
	 */
	if (object_cache_reserve(cache, data->len) < 0 ||
	    git_sizemap_set(cache->map, position, entry) < 0) {
		error = -1;
		goto done;
	}

	/*
	 * Increase the refcount; while this is in the LRU, it will have
	 * a refcount of (at least) one.
	 */
	GIT_REFCOUNT_INC(entry);

	entry->time = git_time_monotonic();

	/* TODO: weird, move position ? call it key? idk */
	entry->position = position;
	entry->prev = cache->newest;
	entry->next = NULL;

	if (cache->newest)
		cache->newest->next = entry;

	cache->newest = entry;

	if (!cache->oldest)
		cache->oldest = entry;

	/* TODO: overflow / sanity checking here */
	cache->used += data->len;

done:
	git_mutex_unlock(&cache->lock);

	if (error < 0)
		git__free(entry);

	return error ? -1 : 0;
}

static void object_cache_dispose(
	struct object_cache *cache)
{
	struct object_cache_entry *entry, *dispose;

	if (git_mutex_lock(&cache->lock) < 0)
		return;

	/* dump_cache(cache); */

	entry = cache->oldest;

	while (entry != NULL) {
		dispose = entry;
		entry = entry->next;

		/* TODO: unnecessary */
		cache->used -= ((struct object_data *)dispose)->len;

		/* TODO: this is useless */
		GIT_ASSERT_WITH_CLEANUP(GIT_REFCOUNT_VAL(dispose) == 1, {});
		git__free(dispose);
	}

	/* TODO: unnecessary */
	if (cache->used != 0)
		abort();

	git_sizemap_free(cache->map);

	git_mutex_unlock(&cache->lock);
	git_mutex_free(&cache->lock);
}


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

/*	printf("--> header cb: %d %d\n", (int)version, (int)entries);*/

	/* TODO: is 50% a good number? not convinced. */
	if (git_sizemap_new(&indexer->positions) < 0 ||
	    git_oidmap_new(&indexer->ids) < 0 ||
	    object_cache_init(&indexer->basecache) < 0 ||
	    git_vector_init(&indexer->objects, entries, objects_cmp) < 0 ||
	    git_vector_init(&indexer->offset_deltas, entries / 2, offset_delta_cmp) < 0 ||
	    git_vector_init(&indexer->ref_deltas, entries / 2, ref_delta_cmp) < 0)
		return -1;

	indexer->started = 1;
	indexer->version = version;
	indexer->entries = entries;

	indexer->progress.total_objects = entries;

	return 0;
}

static int parse_object_start(
	git_object_size_t position,
	git_object_size_t header_size,
	git_object_t type,
	git_object_size_t size,
	void *idx)
{
	git_indexer *indexer = (git_indexer *)idx;
	struct object_entry *entry;

	/* TODO: pool? */
	entry = git__malloc(sizeof(struct object_entry));
	GIT_ERROR_CHECK_ALLOC(entry);

	entry->type = type;
	entry->position = position;
	entry->header_size = header_size;
	entry->size = size;

	indexer->current_object = entry;

	return 0;
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

	/* TODO: we can use compressed_size if we mmap */

	git_oid_cpy(&entry->id, oid);
	entry->crc32 = compressed_crc;

	if (git_sizemap_set(indexer->positions, entry->position, entry) < 0 ||
	    git_oidmap_set(indexer->ids, &entry->id, entry) < 0 ||
	    git_vector_insert(&indexer->objects, entry) < 0)
		return -1;

	indexer->current_object = NULL;

	indexer->progress.received_objects++;
	indexer->progress.indexed_objects++;

	return do_progress_cb(indexer);
}

static int parse_delta_start(
	git_object_size_t position,
	git_object_t type,
	git_object_size_t header_size,
	git_object_size_t size,
	git_oid *delta_ref,
	git_object_size_t delta_offset,
	void *idx)
{
	git_indexer *indexer = (git_indexer *)idx;
	struct delta_entry *entry;

	/* TODO: pool? */
	entry = git__malloc(sizeof(struct delta_entry));
	GIT_ERROR_CHECK_ALLOC(entry);

	/* TODO: avoid double copy - preallocate the entry */
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

	if (git_repository__fsync_gitdir)
		indexer->do_fsync = 1;

	if ((error = git_packfile_parser_init(&indexer->parser, oid_type)) < 0 ||
	    (error = git_str_joinpath(&indexer->base_path, parent_path, "pack")) < 0 ||
	    (error = indexer->packfile_fd = git_futils_mktmp(&indexer->packfile_path,
			indexer->base_path.ptr, indexer->mode)) < 0)
		goto done;

	indexer->parser.packfile_header = parse_packfile_header;
	indexer->parser.object_start = parse_object_start;
	indexer->parser.object_complete = parse_object_complete;
	indexer->parser.delta_start = parse_delta_start;
	indexer->parser.delta_data = parse_delta_data;
	indexer->parser.delta_complete = parse_delta_complete;
	indexer->parser.packfile_complete = parse_packfile_complete;
	indexer->parser.callback_data = indexer;

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
	size_t len)
{
	size_t chunk_len;

	/* TODO: if we're using this to fix thin packs, we shouldn't be incrementing received_bytes. */
	/* although maybe we shouldn't be incrememnting received_bytes at all :'( */

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

	GIT_UNUSED(stats);

	if (!indexer->index_start)
		indexer->index_start = git_time_monotonic();


	/*
	 * Take two passes with the data given to us: first, actually do the
	 * appending to the packfile. Next, do whatever parsing we can.
	 */

	if (append_data(indexer, data, len) < 0)
		return -1;

	if ((error = git_packfile_parser_parse(&indexer->parser, data, len)) < 0)
		return error;

	if (stats)
		memcpy(stats, &indexer->progress, sizeof(git_indexer_progress));

	return 0;
}

/* TODO: this should live somewhere else -- maybe in packfile parser? */
static int load_raw_object(
	struct object_data **out,
	git_indexer *indexer,
	struct resolver_context *resolver_ctx,
	struct object_entry *object)
{
	struct object_data *data = NULL;
	size_t data_remain, raw_position;
	unsigned char *compressed_ptr, *data_ptr;

	/* TODO: overflow checking */
	raw_position = object->position + object->header_size;

	/* TODO: overflow checking */
	/* TODO: pool? */
	data = git__calloc(1, sizeof(struct object_data) + object->size + 1);
	GIT_ERROR_CHECK_ALLOC(data);

	data->len = object->size;
	data->type = object->type;

	data_ptr = data->data;
	data_remain = object->size;

	/* TODO: we need to be more thoughtful about file descriptors for multithreaded unpacking */
	compressed_ptr = indexer->packfile_map + raw_position;

	git_zstream_reset(&resolver_ctx->zstream);

/* TODO: we know where the compressed data ends based on the next offset */
	if (git_zstream_set_input(&resolver_ctx->zstream, compressed_ptr, (indexer->packfile_size - raw_position)) < 0)
		goto on_error;

	while (data_remain && !git_zstream_eos(&resolver_ctx->zstream)) {
		size_t data_written = data_remain;

	    if (git_zstream_get_output(data_ptr, &data_written, &resolver_ctx->zstream) < 0)
			goto on_error;

		data_ptr += data_remain;
		data_remain -= data_written;
	}

	if (data_remain > 0 || !git_zstream_eos(&resolver_ctx->zstream)) {
		git_error_set(GIT_ERROR_INDEXER, "object data did not match expected size");
		goto on_error;
	}

	/* TODO - sanity check type */

	GIT_REFCOUNT_INC(&data->cache_entry);
	*out = data;

	return 0;

on_error:
	git__free(data);
	return -1;
}

static int load_resolved_object(
	struct object_data **out,
	git_indexer *indexer,
	struct resolver_context *resolver_ctx,
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
	/* TODO: make this a constant */
	unsigned char header[64];
	const void *base_data;
	size_t base_position, base_len, header_len;
	git_str deflate_buf = GIT_STR_INIT;
	struct object_entry *entry;
	git_object_t base_type;
	uint32_t base_crc;
	int error;

	printf("out: %p indexer: %p\n", out, indexer);
	printf("fixing thin pack for: %s\n", git_oid_tostr_s(base_id));

	checksum_size = git_hash_size(indexer_hash_algorithm(indexer));

	if (!indexer->odb)
		return GIT_ENOTFOUND;

	if ((error = git_odb_read(&base, indexer->odb, base_id)) < 0)
		goto done;

	/* TODO: take a write lock on the packfile */

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
	    append_data(indexer, header, header_len) < 0 ||
	    append_data(indexer, deflate_buf.ptr, deflate_buf.size) < 0) {
		error = -1;
		goto done;
	}

	base_crc = crc32(0L, Z_NULL, 0);
	base_crc = crc32(base_crc, header, header_len);
	base_crc = crc32(base_crc, (const unsigned char *)deflate_buf.ptr, deflate_buf.size);

	/* Add this to our record-keeping */

	/* TODO: pool? */
	entry = git__malloc(sizeof(struct object_entry));
	GIT_ERROR_CHECK_ALLOC(entry);

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

	printf("type: %d\n", git_odb_object_type(base));
	printf("      %.*s\n", (int)base_len, (char *)base_data);

	indexer->has_thin_entries = 1;

	*out = entry;
	error = 0;

done:
	/* unlock packfile */

	git_str_dispose(&deflate_buf);
	return error;
}

GIT_INLINE(int) load_resolved_delta_object(
	struct object_data **out,
	git_indexer *indexer,
	struct resolver_context *resolver_ctx,
	struct object_entry *_delta,
	struct object_entry *base)
{
	struct delta_entry *delta = (struct delta_entry *)_delta;
	struct object_data *base_data, *delta_data, *result_data;
	size_t base_size, result_size;
	int error;

	/* load the base */
	if (!base && delta->object.type == GIT_OBJECT_OFS_DELTA) {
		/* TODO readlock */
		base = git_sizemap_get(indexer->positions, delta->base.ofs_position);

		if (!base) {
			git_error_set(GIT_ERROR_INDEXER, "corrupt packfile - no object at offset position %llu", delta->base.ofs_position);
			return -1;
		}
	} else if (!base && delta->object.type == GIT_OBJECT_REF_DELTA) {
		/* TODO readlock */
		base = git_oidmap_get(indexer->ids, &delta->base.ref_id);

		if (!base && resolver_ctx->fix_thin_packs &&
			(error = insert_thin_base(&base, indexer, &delta->base.ref_id)) < 0)
				return error;

		if (!base) {
			git_error_set(GIT_ERROR_INDEXER, "corrupt packfile - no object id %s", git_oid_tostr_s(&delta->base.ref_id));
			return -1;
		}
	}

	GIT_ASSERT(base);


	/* TODO: need any locks here ? */
	if (load_resolved_object(&base_data, indexer, resolver_ctx, base, NULL) < 0 ||
	    load_raw_object(&delta_data, indexer, resolver_ctx, _delta) < 0 ||
		git_delta_read_header(&base_size, &result_size,
			delta_data->data, delta_data->len) < 0)
		return -1;

	/* TODO: overflow check */
	/* TODO: pool? */
	result_data = git__calloc(1, sizeof(struct object_data) + result_size + 1);
	result_data->data[result_size] = '\0';
	result_data->len = result_size;
	result_data->type = base_data->type;

	if (git_delta_apply_to_buf(result_data->data, result_data->len,
			base_data->data, base_data->len,
			delta_data->data, delta_data->len) < 0) {
		git__free(result_data);
		return -1;
	}

	GIT_REFCOUNT_DEC(&base_data->cache_entry, object_cache_free);
	GIT_REFCOUNT_DEC(&delta_data->cache_entry, object_cache_free);
	GIT_REFCOUNT_INC(&result_data->cache_entry);
	*out = result_data;

	return 0;
}

static int load_resolved_object(
	struct object_data **out,
	git_indexer *indexer,
	struct resolver_context *resolver_ctx,
	struct object_entry *object,
	struct object_entry *base)
{
	struct object_data *data;
	int error;

	indexer->object_lookups++;

	/* cache lookup */

	if (object->type == GIT_OBJECT_OFS_DELTA &&
	    (data = object_cache_get(&indexer->basecache, object->position)) != NULL) {
		indexer->cache_hits++;
		*out = data;
		return 0;
	}

	if (object->type == GIT_OBJECT_REF_DELTA || object->type == GIT_OBJECT_OFS_DELTA) {
		error = load_resolved_delta_object(&data, indexer, resolver_ctx, object, base);

		if (error < 0)
			return error;
	} else {
		if (load_raw_object(&data, indexer, resolver_ctx, object) < 0)
			return -1;
	}

	/* cache set */
	if (object_cache_put(&indexer->basecache, object->position, data) < 0)
		return -1;

	*out = data;
	return 0;
}

GIT_INLINE(int) resolve_delta(
	git_indexer *indexer,
	struct resolver_context *resolver_ctx,
	struct delta_entry *delta,
	struct object_entry *base)
{
	struct object_data *result;
	char header[64];
	size_t header_len;
	int error;

	if ((error = load_resolved_object(&result, indexer, resolver_ctx, (struct object_entry *)delta, base)) < 0)
		return error;

	if (git_hash_init(&resolver_ctx->hash_ctx) < 0 ||
	    git_odb__format_object_header(&header_len, header, sizeof(header), result->len, result->type) < 0 ||
	    git_hash_update(&resolver_ctx->hash_ctx, header, header_len) < 0 ||
	    git_hash_update(&resolver_ctx->hash_ctx, result->data, result->len) < 0 ||
	    git_hash_final(delta->object.id.id, &resolver_ctx->hash_ctx) < 0) {
		error = -1;
		goto done;
	}

	delta->final_type = result->type;

#ifdef GIT_EXPERIMENTAL_SHA256
	delta->object.id.type = indexer->oid_type;
#endif

	/* TODO: need a lock here !*/
	if ((error = git_oidmap_set(indexer->ids, &delta->object.id, &delta->object)) < 0)
		goto done;

	/* TODO: atomic updates */
	indexer->progress.indexed_deltas++;
	indexer->progress.indexed_objects++;

	/* TODO: hmm */

	printf("%d / %d\n", indexer->progress.indexed_deltas, indexer->progress.indexed_objects);

	if (resolver_ctx->send_progress_updates)
		error = do_progress_cb(indexer);

done:
	GIT_REFCOUNT_DEC(&result->cache_entry, object_cache_free);
	return error;
}

static int resolve_offset_partition(
	git_indexer *indexer,
	struct offset_resolver_context *resolver_ctx)
{
	struct object_entry *object_entry;
	struct delta_entry *delta_start, *delta_entry;
	size_t object_idx, delta_idx = resolver_ctx->start_idx;
	int error;

	/* TODO figure out some way to ferry git errors back to the main thread */
	/* TODO: only fire events on the main thread */

	printf("thread %zu: resolving %zu - %zu\n", resolver_ctx->thread_number,
		resolver_ctx->start_idx, resolver_ctx->end_idx);


	delta_start = git_vector_get(&indexer->offset_deltas, delta_idx);

	/*
	 * At this point, our deltas are sorted by the positions of their
	 * bases. Loop over the objects and applying all the deltas that
	 * use this object as their base.
	 */
	git_vector_foreach(&indexer->objects, object_idx, object_entry) {
		printf("%llu / %llu\n", object_entry->position, delta_start->base.ofs_position);


		/* We may not have gotten to this delta yet */
		/* TODO: start at the right object instead / bsearch */
		if (object_entry->position < delta_start->base.ofs_position)
			continue;

		while (delta_idx < resolver_ctx->end_idx) {
			delta_entry = git_vector_get(&indexer->offset_deltas, delta_idx);

			printf("delta_entry: %p / type: %d\n", delta_entry, delta_entry->object.type);

			/* TODO */
			GIT_ASSERT(delta_entry->object.type == GIT_OBJECT_OFS_DELTA);

			/*
			 * We've hit the end of the deltas that use this object
			 * as a base.
			 */
			if (delta_entry->base.ofs_position > object_entry->position)
				break;

			/* TODO - keep or drop? unnecessary but let's get it right */
			GIT_ASSERT(delta_entry->base.ofs_position >= object_entry->position);


		printf("thread %zu: resolving %zu %p\n", resolver_ctx->thread_number, delta_idx, delta_entry);




			if ((error = resolve_delta(indexer, &resolver_ctx->base, delta_entry, object_entry)) < 0) {
				printf("failed! [%lu] [%s]\n", resolver_ctx->thread_number, git_error_last()->message);
				return error;
			}

			else {
/*				printf("skipping %llu [%lu]\n", delta_entry->base.ofs_position, resolver_ctx->idx); */
			}

			delta_idx++;
		}
	}

	return do_progress_cb(indexer);
}

static void *resolve_offset_thread(void *_ctx)
{
	struct offset_resolver_context *resolver_ctx = (struct offset_resolver_context *)_ctx;
	ssize_t result = resolve_offset_partition(resolver_ctx->base.indexer, resolver_ctx);
	return (void *)result;
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

	printf("writing header...\n");

	/* TODO: configurable file mode */
	if (git_hash_ctx_init(&hash_ctx, hash_type) < 0 ||
	    git_hash_init(&hash_ctx) < 0 ||
	    git_str_join(&indexer->index_path, '.', indexer->packfile_path.ptr, "idx") < 0 ||
	    (fd = p_open(indexer->index_path.ptr, O_RDWR|O_CREAT, indexer->mode)) < 0 ||
		(fp = fdopen(fd, "w")) == NULL)
		goto on_error;

	if (hash_and_write(fp, &hash_ctx, "\377tOc\000\000\000\002", 8) < 0)
		goto on_error;

	printf("writing fanout...\n");

	/* Write fanout section */
	do {
		while (i < git_vector_length(&indexer->objects) &&
		       (entry = indexer->objects.contents[i]) &&
			   entry->id.id[0] == fanout) {
			/* TODO; overflow checking? */
			/* we don't really need it here if we clamp the number of objects to uint32 elsewhere -- and we should. */
			fanout_count++;
			i++;
		}

		nl = htonl(fanout_count);

		if (hash_and_write(fp, &hash_ctx, &nl, 4) < 0)
			goto on_error;
	} while (fanout++ < 0xff);

	printf("writing oids...\n");

	/* Write object IDs */
	oid_size = git_oid_size(indexer->oid_type);
	git_vector_foreach(&indexer->objects, i, entry) {
		if (hash_and_write(fp, &hash_ctx, entry->id.id, oid_size) < 0)
			goto on_error;
	}

	printf("writing crcs...\n");

	/* Write the CRC32s */
	git_vector_foreach(&indexer->objects, i, entry) {
		nl = htonl(entry->crc32);

		if (hash_and_write(fp, &hash_ctx, &nl, sizeof(uint32_t)) < 0)
			goto on_error;
	}

	printf("writing small offsets...\n");

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

	printf("writing long offsets...\n");

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
	git_thread thread[THREADS_MAX];
	struct offset_resolver_context resolver_ctx[THREADS_MAX] = { { { NULL } } } ;
	size_t threads_cnt = THREADS_DEFAULT;
	size_t deltas_cnt, partition_size, i;
	int error;

	if ((deltas_cnt = git_vector_length(&indexer->offset_deltas)) == 0)
		return 0;

	git_vector_sort(&indexer->offset_deltas);

	/* Each thread will handle a number of objects. */
	partition_size = (threads_cnt > deltas_cnt) ?
		deltas_cnt : deltas_cnt / threads_cnt;

	for (i = 0; i < threads_cnt; i++) {
		size_t partition_start = (i * partition_size);

		resolver_ctx[i].thread_number = i;

		resolver_ctx[i].start_idx = partition_start;
		resolver_ctx[i].end_idx = (i < threads_cnt - 1) ?
			(partition_start + partition_size) : deltas_cnt;

		resolver_ctx[i].base.indexer = indexer;
		resolver_ctx[i].base.send_progress_updates = (i == 0);

		/* TODO: free stuff on cleanup */
		if (git_hash_ctx_init(&resolver_ctx[i].base.hash_ctx, git_oid_algorithm(indexer->oid_type)) < 0 ||
		    git_zstream_init(&resolver_ctx[i].base.zstream, GIT_ZSTREAM_INFLATE) < 0)
			return -1;

		if (resolver_ctx[i].end_idx == deltas_cnt) {
			threads_cnt = i + 1;
			break;
		}
	}

	/* Start background threads 1-n (thread 0 is this thread) */
	for (i = 1; i < threads_cnt; i++) {
		if (git_thread_create(&thread[i], resolve_offset_thread, &resolver_ctx[i]) != 0) {
			git_error_set(GIT_ERROR_THREAD, "unable to create thread");
			return -1;
		}
	}

	error = resolve_offset_partition(indexer, &resolver_ctx[0]);

	for (i = 1; i < threads_cnt; i++) {
		void *result;

		if (git_thread_join(&thread[i], &result) != 0) {
			git_error_set(GIT_ERROR_THREAD, "unable to join thread");
			error = -1;
		}

		if ((ssize_t)result != 0)
			error = -1;
	}

	return error;
}

static int resolve_ref_deltas(git_indexer *indexer)
{
	struct resolver_context resolver_ctx = { NULL };
	struct delta_entry *delta_entry;
	size_t delta_idx;
	bool progress = false;
	int error;

	resolver_ctx.indexer = indexer;
	resolver_ctx.send_progress_updates = 1;
	resolver_ctx.fix_thin_packs = 1;

	/* TODO: free stuff on cleanup */
	if (git_hash_ctx_init(&resolver_ctx.hash_ctx, git_oid_algorithm(indexer->oid_type)) < 0 ||
	    git_zstream_init(&resolver_ctx.zstream, GIT_ZSTREAM_INFLATE) < 0)
		return -1;

	do {
		progress = false;

		git_vector_foreach(&indexer->ref_deltas, delta_idx, delta_entry) {
			/* Skip this if we've already resolved this. */
			if (delta_entry->final_type)
				continue;

			printf("base: %s\n", git_oid_tostr_s(&delta_entry->base.ref_id));

			error = resolve_delta(indexer, &resolver_ctx, delta_entry, NULL);

			if (error == GIT_ENOTFOUND) {
				continue;
			} else if (error < 0) {
				printf("failed! [%s]\n", git_error_last()->message);
				return error;
			}

			printf("indexed: %s\n", git_oid_tostr_s(&delta_entry->object.id));

			progress = true;
		}
	} while (progress);

	printf("done\n");

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

		git_error_set(GIT_ERROR_INDEXER, "could not find base object '%s' to resolve delta", git_oid_tostr_s(&delta_entry->base.ref_id));
		return -1;
	}

	/* Update the header to include the number of injected objects. */
	if (!indexer->has_thin_entries)
		return 0;

	hash_type = git_oid_algorithm(indexer->oid_type);

	if (git_hash_ctx_init(&hash_ctx, hash_type) < 0)
		return -1;

// TODO: don't use the progress object, can we keep a local copy?
	/* TODO: check for overflow */
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

printf("header size: %lu / trailer size: %lu\n", sizeof(struct git_pack_header), git_hash_size(hash_type));
printf("Content size is: %lu\n", content_size);

	while (content_size > 0 && (ret = p_read(indexer->packfile_fd, buf, min(content_size, sizeof(buf)))) > 0) {
		printf("-- read %lu / current position: %lu / %lx\n", ret, p_lseek(indexer->packfile_fd, 0, SEEK_CUR), p_lseek(indexer->packfile_fd, 0, SEEK_CUR));

		if (git_hash_update(&hash_ctx, buf, ret) < 0)
			return -1;

		content_size -= ret;
	}

	if (ret < 0) {
		git_error_set(GIT_ERROR_OS, "could not read packfile to rehash");
		return -1;
	}

	if (git_hash_final(indexer->trailer, &hash_ctx) < 0) {
		git_error_set(GIT_ERROR_OS, "could not rehash packfile");
		return -1;
	}

	return append_data(indexer, indexer->trailer, git_hash_size(hash_type));
}

int git_indexer_commit(git_indexer *indexer, git_indexer_progress *stats)
{
	struct object_entry *entry;
	git_str packfile_path = GIT_STR_INIT, index_path = GIT_STR_INIT;
	size_t i;
	int error;

	GIT_ASSERT_ARG(indexer);

	if (!indexer->complete) {
		git_error_set(GIT_ERROR_INDEXER, "incomplete packfile");
		goto on_error;
	}

	/* Freeze the number of deltas */
	indexer->progress.total_deltas =
		indexer->progress.total_objects - indexer->progress.indexed_objects;

	/* TODO: why is this here? we need to update this when we're done. but maybe we need to set it up at the beginning for callers...? what does do_progress_cb actually do? hmm... */
	if (stats)
		memcpy(stats, &indexer->progress, sizeof(git_indexer_progress));

	if ((error = do_progress_cb(indexer)) != 0)
		return error;


	/* TODO: deal with the timing statistics better */
	indexer->index_end = git_time_monotonic();
	/*printf("elapsed: %llu\n", (indexer->index_end - indexer->index_start));*/

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

/* TODO: mixing up hashes and oids here - probably elsewhere too */
	if (git_oid__fromraw(&indexer->trailer_oid, indexer->trailer, indexer->oid_type) < 0 ||
	    git_hash_fmt(indexer->trailer_name, indexer->trailer, git_oid_size(indexer->oid_type)) < 0)
		goto on_error;

	if (indexer->do_fsync && p_fsync(indexer->packfile_fd) < 0) {
		git_error_set(GIT_ERROR_OS, "failed to fsync packfile");
		goto on_error;
	}

	p_close(indexer->packfile_fd);
	indexer->packfile_fd = -1;

	/* TODO: zap */
	if (0) {
	git_vector_foreach(&indexer->objects, i, entry) {
		git_object_t type = git_object__is_delta(entry->type) ?
			((struct delta_entry *)entry)->final_type : entry->type;

		printf("%s %-6s ... ... %llu",
			git_oid_tostr_s(&entry->id),
			git_object_type2string(type),
			entry->position);
		}

		printf("\n");
	}
	/* TODO: /zap */

	printf("sorting...\n");
	git_vector_sort(&indexer->objects);

	printf("writing...\n");
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



	{
/*    git_hash_fmt(foo, packfile_trailer, git_oid_size(indexer->oid_type)) < 0) */
	printf("%s / %s\n", git_oid_tostr_s(&indexer->trailer_oid), indexer->trailer_name);
}

	printf("\n\nobject lookups: %lu / cache hits: %lu\n\n", indexer->object_lookups, indexer->cache_hits);

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

	git_str_dispose(&indexer->index_path);
	git_str_dispose(&indexer->packfile_path);
	git_str_dispose(&indexer->base_path);
	object_cache_dispose(&indexer->basecache);
	git_sizemap_free(indexer->positions);
	git_oidmap_free(indexer->ids);
	git_vector_free(&indexer->offset_deltas);
	git_vector_free(&indexer->ref_deltas);
	git_vector_free_deep(&indexer->objects);
	git_packfile_parser_dispose(&indexer->parser);
	git__free(indexer);
}
