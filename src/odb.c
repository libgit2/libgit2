/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include <zlib.h>
#include "git2/object.h"
#include "fileops.h"
#include "hash.h"
#include "odb.h"
#include "delta-apply.h"
#include "filter.h"

#include "git2/odb_backend.h"
#include "git2/oid.h"

#define GIT_ALTERNATES_FILE "info/alternates"

/* TODO: is this correct? */
#define GIT_LOOSE_PRIORITY 2
#define GIT_PACKED_PRIORITY 1

#define GIT_ALTERNATES_MAX_DEPTH 5

typedef struct
{
	git_odb_backend *backend;
	int priority;
	int is_alternate;
} backend_internal;

static int load_alternates(git_odb *odb, const char *objects_dir, int alternate_depth);

int git_odb__format_object_header(char *hdr, size_t n, size_t obj_len, git_otype obj_type)
{
	const char *type_str = git_object_type2string(obj_type);
	int len = p_snprintf(hdr, n, "%s %"PRIuZ, type_str, obj_len);
	assert(len > 0 && len <= (int)n);
	return len+1;
}

int git_odb__hashobj(git_oid *id, git_rawobj *obj)
{
	git_buf_vec vec[2];
	char header[64];
	int hdrlen;

	assert(id && obj);

	if (!git_object_typeisloose(obj->type))
		return -1;
	if (!obj->data && obj->len != 0)
		return -1;

	hdrlen = git_odb__format_object_header(header, sizeof(header), obj->len, obj->type);

	vec[0].data = header;
	vec[0].len = hdrlen;
	vec[1].data = obj->data;
	vec[1].len = obj->len;

	git_hash_vec(id, vec, 2);

	return 0;
}


static git_odb_object *new_odb_object(const git_oid *oid, git_rawobj *source)
{
	git_odb_object *object = git__malloc(sizeof(git_odb_object));
	memset(object, 0x0, sizeof(git_odb_object));

	git_oid_cpy(&object->cached.oid, oid);
	memcpy(&object->raw, source, sizeof(git_rawobj));

	return object;
}

static void free_odb_object(void *o)
{
	git_odb_object *object = (git_odb_object *)o;

	if (object != NULL) {
		git__free(object->raw.data);
		git__free(object);
	}
}

const git_oid *git_odb_object_id(git_odb_object *object)
{
	return &object->cached.oid;
}

const void *git_odb_object_data(git_odb_object *object)
{
	return object->raw.data;
}

size_t git_odb_object_size(git_odb_object *object)
{
	return object->raw.len;
}

git_otype git_odb_object_type(git_odb_object *object)
{
	return object->raw.type;
}

void git_odb_object_free(git_odb_object *object)
{
	if (object == NULL)
		return;

	git_cached_obj_decref((git_cached_obj *)object, &free_odb_object);
}

int git_odb__hashfd(git_oid *out, git_file fd, size_t size, git_otype type)
{
	int hdr_len;
	char hdr[64], buffer[2048];
	git_hash_ctx ctx;
	ssize_t read_len = 0;
	int error = 0;

	if (!git_object_typeisloose(type)) {
		giterr_set(GITERR_INVALID, "Invalid object type for hash");
		return -1;
	}

	if ((error = git_hash_ctx_init(&ctx)) < 0)
		return -1;

	hdr_len = git_odb__format_object_header(hdr, sizeof(hdr), size, type);

	if ((error = git_hash_update(&ctx, hdr, hdr_len)) < 0)
		goto done;

	while (size > 0 && (read_len = p_read(fd, buffer, sizeof(buffer))) > 0) {
		if ((error = git_hash_update(&ctx, buffer, read_len)) < 0)
			goto done;

		size -= read_len;
	}

	/* If p_read returned an error code, the read obviously failed.
	 * If size is not zero, the file was truncated after we originally
	 * stat'd it, so we consider this a read failure too */
	if (read_len < 0 || size > 0) {
		giterr_set(GITERR_OS, "Error reading file for hashing");
		error = -1;

		goto done;
		return -1;
	}

	error = git_hash_final(out, &ctx);

done:
	git_hash_ctx_cleanup(&ctx);
	return error;
}

int git_odb__hashfd_filtered(
	git_oid *out, git_file fd, size_t size, git_otype type, git_vector *filters)
{
	int error;
	git_buf raw = GIT_BUF_INIT;
	git_buf filtered = GIT_BUF_INIT;

	if (!filters || !filters->length)
		return git_odb__hashfd(out, fd, size, type);

	/* size of data is used in header, so we have to read the whole file
	 * into memory to apply filters before beginning to calculate the hash
	 */

	if (!(error = git_futils_readbuffer_fd(&raw, fd, size)))
		error = git_filters_apply(&filtered, &raw, filters);

	git_buf_free(&raw);

	if (!error)
		error = git_odb_hash(out, filtered.ptr, filtered.size, type);

	git_buf_free(&filtered);

	return error;
}

int git_odb__hashlink(git_oid *out, const char *path)
{
	struct stat st;
	git_off_t size;
	int result;

	if (git_path_lstat(path, &st) < 0)
		return -1;

	size = st.st_size;

	if (!git__is_sizet(size)) {
		giterr_set(GITERR_OS, "File size overflow for 32-bit systems");
		return -1;
	}

	if (S_ISLNK(st.st_mode)) {
		char *link_data;
		ssize_t read_len;

		link_data = git__malloc((size_t)(size + 1));
		GITERR_CHECK_ALLOC(link_data);

		read_len = p_readlink(path, link_data, (size_t)size);
		link_data[size] = '\0';
		if (read_len != (ssize_t)size) {
			giterr_set(GITERR_OS, "Failed to read symlink data for '%s'", path);
			return -1;
		}

		result = git_odb_hash(out, link_data, (size_t)size, GIT_OBJ_BLOB);
		git__free(link_data);
	} else {
		int fd = git_futils_open_ro(path);
		if (fd < 0)
			return -1;
		result = git_odb__hashfd(out, fd, (size_t)size, GIT_OBJ_BLOB);
		p_close(fd);
	}

	return result;
}

int git_odb_hashfile(git_oid *out, const char *path, git_otype type)
{
	git_off_t size;
	int result, fd = git_futils_open_ro(path);
	if (fd < 0)
		return fd;

	if ((size = git_futils_filesize(fd)) < 0 || !git__is_sizet(size)) {
		giterr_set(GITERR_OS, "File size overflow for 32-bit systems");
		p_close(fd);
		return -1;
	}

	result = git_odb__hashfd(out, fd, (size_t)size, type);
	p_close(fd);
	return result;
}

int git_odb_hash(git_oid *id, const void *data, size_t len, git_otype type)
{
	git_rawobj raw;

	assert(id);

	raw.data = (void *)data;
	raw.len = len;
	raw.type = type;

	return git_odb__hashobj(id, &raw);
}

/**
 * FAKE WSTREAM
 */

typedef struct {
	git_odb_stream stream;
	char *buffer;
	size_t size, written;
	git_otype type;
} fake_wstream;

static int fake_wstream__fwrite(git_oid *oid, git_odb_stream *_stream)
{
	fake_wstream *stream = (fake_wstream *)_stream;
	return _stream->backend->write(oid, _stream->backend, stream->buffer, stream->size, stream->type);
}

static int fake_wstream__write(git_odb_stream *_stream, const char *data, size_t len)
{
	fake_wstream *stream = (fake_wstream *)_stream;

	if (stream->written + len > stream->size)
		return -1;

	memcpy(stream->buffer + stream->written, data, len);
	stream->written += len;
	return 0;
}

static void fake_wstream__free(git_odb_stream *_stream)
{
	fake_wstream *stream = (fake_wstream *)_stream;

	git__free(stream->buffer);
	git__free(stream);
}

static int init_fake_wstream(git_odb_stream **stream_p, git_odb_backend *backend, size_t size, git_otype type)
{
	fake_wstream *stream;

	stream = git__calloc(1, sizeof(fake_wstream));
	GITERR_CHECK_ALLOC(stream);

	stream->size = size;
	stream->type = type;
	stream->buffer = git__malloc(size);
	if (stream->buffer == NULL) {
		git__free(stream);
		return -1;
	}

	stream->stream.backend = backend;
	stream->stream.read = NULL; /* read only */
	stream->stream.write = &fake_wstream__write;
	stream->stream.finalize_write = &fake_wstream__fwrite;
	stream->stream.free = &fake_wstream__free;
	stream->stream.mode = GIT_STREAM_WRONLY;

	*stream_p = (git_odb_stream *)stream;
	return 0;
}

/***********************************************************
 *
 * OBJECT DATABASE PUBLIC API
 *
 * Public calls for the ODB functionality
 *
 ***********************************************************/

static int backend_sort_cmp(const void *a, const void *b)
{
	const backend_internal *backend_a = (const backend_internal *)(a);
	const backend_internal *backend_b = (const backend_internal *)(b);

	if (backend_a->is_alternate == backend_b->is_alternate)
		return (backend_b->priority - backend_a->priority);

	return backend_a->is_alternate ? 1 : -1;
}

int git_odb_new(git_odb **out)
{
	git_odb *db = git__calloc(1, sizeof(*db));
	GITERR_CHECK_ALLOC(db);

	if (git_cache_init(&db->cache, GIT_DEFAULT_CACHE_SIZE, &free_odb_object) < 0 ||
		git_vector_init(&db->backends, 4, backend_sort_cmp) < 0)
	{
		git__free(db);
		return -1;
	}

	*out = db;
	GIT_REFCOUNT_INC(db);
	return 0;
}

static int add_backend_internal(git_odb *odb, git_odb_backend *backend, int priority, int is_alternate)
{
	backend_internal *internal;

	assert(odb && backend);

	GITERR_CHECK_VERSION(backend, GIT_ODB_BACKEND_VERSION, "git_odb_backend");

	/* Check if the backend is already owned by another ODB */
	assert(!backend->odb || backend->odb == odb);

	internal = git__malloc(sizeof(backend_internal));
	GITERR_CHECK_ALLOC(internal);

	internal->backend = backend;
	internal->priority = priority;
	internal->is_alternate = is_alternate;

	if (git_vector_insert(&odb->backends, internal) < 0) {
		git__free(internal);
		return -1;
	}

	git_vector_sort(&odb->backends);
	internal->backend->odb = odb;
	return 0;
}

int git_odb_add_backend(git_odb *odb, git_odb_backend *backend, int priority)
{
	return add_backend_internal(odb, backend, priority, 0);
}

int git_odb_add_alternate(git_odb *odb, git_odb_backend *backend, int priority)
{
	return add_backend_internal(odb, backend, priority, 1);
}

static int add_default_backends(git_odb *db, const char *objects_dir, int as_alternates, int alternate_depth)
{
	git_odb_backend *loose, *packed;

	/* add the loose object backend */
	if (git_odb_backend_loose(&loose, objects_dir, -1, 0) < 0 ||
		add_backend_internal(db, loose, GIT_LOOSE_PRIORITY, as_alternates) < 0)
		return -1;

	/* add the packed file backend */
	if (git_odb_backend_pack(&packed, objects_dir) < 0 ||
		add_backend_internal(db, packed, GIT_PACKED_PRIORITY, as_alternates) < 0)
		return -1;

	return load_alternates(db, objects_dir, alternate_depth);
}

static int load_alternates(git_odb *odb, const char *objects_dir, int alternate_depth)
{
	git_buf alternates_path = GIT_BUF_INIT;
	git_buf alternates_buf = GIT_BUF_INIT;
	char *buffer;
	const char *alternate;
	int result = 0;

	/* Git reports an error, we just ignore anything deeper */
	if (alternate_depth > GIT_ALTERNATES_MAX_DEPTH) {
		return 0;
	}

	if (git_buf_joinpath(&alternates_path, objects_dir, GIT_ALTERNATES_FILE) < 0)
		return -1;

	if (git_path_exists(alternates_path.ptr) == false) {
		git_buf_free(&alternates_path);
		return 0;
	}

	if (git_futils_readbuffer(&alternates_buf, alternates_path.ptr) < 0) {
		git_buf_free(&alternates_path);
		return -1;
	}

	buffer = (char *)alternates_buf.ptr;

	/* add each alternate as a new backend; one alternate per line */
	while ((alternate = git__strtok(&buffer, "\r\n")) != NULL) {
		if (*alternate == '\0' || *alternate == '#')
			continue;

		/*
		 * Relative path: build based on the current `objects`
		 * folder. However, relative paths are only allowed in
		 * the current repository.
		 */
		if (*alternate == '.' && !alternate_depth) {
			if ((result = git_buf_joinpath(&alternates_path, objects_dir, alternate)) < 0)
				break;
			alternate = git_buf_cstr(&alternates_path);
		}

		if ((result = add_default_backends(odb, alternate, 1, alternate_depth + 1)) < 0)
			break;
	}

	git_buf_free(&alternates_path);
	git_buf_free(&alternates_buf);

	return result;
}

int git_odb_add_disk_alternate(git_odb *odb, const char *path)
{
	return add_default_backends(odb, path, 1, 0);
}

int git_odb_open(git_odb **out, const char *objects_dir)
{
	git_odb *db;

	assert(out && objects_dir);

	*out = NULL;

	if (git_odb_new(&db) < 0)
		return -1;

	if (add_default_backends(db, objects_dir, 0, 0) < 0) {
		git_odb_free(db);
		return -1;
	}

	*out = db;
	return 0;
}

static void odb_free(git_odb *db)
{
	size_t i;

	for (i = 0; i < db->backends.length; ++i) {
		backend_internal *internal = git_vector_get(&db->backends, i);
		git_odb_backend *backend = internal->backend;

		if (backend->free) backend->free(backend);
		else git__free(backend);

		git__free(internal);
	}

	git_vector_free(&db->backends);
	git_cache_free(&db->cache);
	git__free(db);
}

void git_odb_free(git_odb *db)
{
	if (db == NULL)
		return;

	GIT_REFCOUNT_DEC(db, odb_free);
}

int git_odb_exists(git_odb *db, const git_oid *id)
{
	git_odb_object *object;
	size_t i;
	bool found = false;
	bool refreshed = false;

	assert(db && id);

	if ((object = git_cache_get(&db->cache, id)) != NULL) {
		git_odb_object_free(object);
		return (int)true;
	}

attempt_lookup:
	for (i = 0; i < db->backends.length && !found; ++i) {
		backend_internal *internal = git_vector_get(&db->backends, i);
		git_odb_backend *b = internal->backend;

		if (b->exists != NULL)
			found = b->exists(b, id);
	}

	if (!found && !refreshed) {
		if (git_odb_refresh(db) < 0) {
			giterr_clear();
			return (int)false;
		}

		refreshed = true;
		goto attempt_lookup;
	}

	return (int)found;
}

int git_odb_read_header(size_t *len_p, git_otype *type_p, git_odb *db, const git_oid *id)
{
	int error;
	git_odb_object *object;

	error = git_odb__read_header_or_object(&object, len_p, type_p, db, id);

	if (object)
		git_odb_object_free(object);

	return error;
}

int git_odb__read_header_or_object(
	git_odb_object **out, size_t *len_p, git_otype *type_p,
	git_odb *db, const git_oid *id)
{
	size_t i;
	int error = GIT_ENOTFOUND;
	git_odb_object *object;

	assert(db && id && out && len_p && type_p);

	if ((object = git_cache_get(&db->cache, id)) != NULL) {
		*len_p = object->raw.len;
		*type_p = object->raw.type;
		*out = object;
		return 0;
	}

	*out = NULL;

	for (i = 0; i < db->backends.length && error < 0; ++i) {
		backend_internal *internal = git_vector_get(&db->backends, i);
		git_odb_backend *b = internal->backend;

		if (b->read_header != NULL)
			error = b->read_header(len_p, type_p, b, id);
	}

	if (!error || error == GIT_PASSTHROUGH)
		return 0;

	/*
	 * no backend could read only the header.
	 * try reading the whole object and freeing the contents
	 */
	if ((error = git_odb_read(&object, db, id)) < 0)
		return error; /* error already set - pass along */

	*len_p = object->raw.len;
	*type_p = object->raw.type;
	*out = object;

	return 0;
}

int git_odb_read(git_odb_object **out, git_odb *db, const git_oid *id)
{
	size_t i;
	int error;
	bool refreshed = false;
	git_rawobj raw;

	assert(out && db && id);

	if (db->backends.length == 0) {
		giterr_set(GITERR_ODB, "Failed to lookup object: no backends loaded");
		return GIT_ENOTFOUND;
	}

	*out = git_cache_get(&db->cache, id);
	if (*out != NULL)
		return 0;

attempt_lookup:
	error = GIT_ENOTFOUND;

	for (i = 0; i < db->backends.length && error < 0; ++i) {
		backend_internal *internal = git_vector_get(&db->backends, i);
		git_odb_backend *b = internal->backend;

		if (b->read != NULL)
			error = b->read(&raw.data, &raw.len, &raw.type, b, id);
	}

	if (error == GIT_ENOTFOUND && !refreshed) {
		if ((error = git_odb_refresh(db)) < 0)
			return error;

		refreshed = true;
		goto attempt_lookup;
	}

	if (error && error != GIT_PASSTHROUGH)
		return error;

	*out = git_cache_try_store(&db->cache, new_odb_object(id, &raw));
	return 0;
}

int git_odb_read_prefix(
	git_odb_object **out, git_odb *db, const git_oid *short_id, size_t len)
{
	size_t i;
	int error = GIT_ENOTFOUND;
	git_oid found_full_oid = {{0}};
	git_rawobj raw;
	void *data = NULL;
	bool found = false, refreshed = false;

	assert(out && db);

	if (len < GIT_OID_MINPREFIXLEN)
		return git_odb__error_ambiguous("prefix length too short");

	if (len > GIT_OID_HEXSZ)
		len = GIT_OID_HEXSZ;

	if (len == GIT_OID_HEXSZ) {
		*out = git_cache_get(&db->cache, short_id);
		if (*out != NULL)
			return 0;
	}

attempt_lookup:
	for (i = 0; i < db->backends.length; ++i) {
		backend_internal *internal = git_vector_get(&db->backends, i);
		git_odb_backend *b = internal->backend;

		if (b->read_prefix != NULL) {
			git_oid full_oid;
			error = b->read_prefix(&full_oid, &raw.data, &raw.len, &raw.type, b, short_id, len);
			if (error == GIT_ENOTFOUND || error == GIT_PASSTHROUGH)
				continue;

			if (error)
				return error;

			git__free(data);
			data = raw.data;

			if (found && git_oid_cmp(&full_oid, &found_full_oid))
				return git_odb__error_ambiguous("multiple matches for prefix");

			found_full_oid = full_oid;
			found = true;
		}
	}

	if (!found && !refreshed) {
		if ((error = git_odb_refresh(db)) < 0)
			return error;

		refreshed = true;
		goto attempt_lookup;
	}

	if (!found)
		return git_odb__error_notfound("no match for prefix", short_id);

	*out = git_cache_try_store(&db->cache, new_odb_object(&found_full_oid, &raw));
	return 0;
}

int git_odb_foreach(git_odb *db, git_odb_foreach_cb cb, void *payload)
{
	unsigned int i;
	backend_internal *internal;

	git_vector_foreach(&db->backends, i, internal) {
		git_odb_backend *b = internal->backend;
		int error = b->foreach(b, cb, payload);
		if (error < 0)
			return error;
	}

	return 0;
}

int git_odb_write(
	git_oid *oid, git_odb *db, const void *data, size_t len, git_otype type)
{
	size_t i;
	int error = GIT_ERROR;
	git_odb_stream *stream;

	assert(oid && db);

	git_odb_hash(oid, data, len, type);
	if (git_odb_exists(db, oid))
		return 0;

	for (i = 0; i < db->backends.length && error < 0; ++i) {
		backend_internal *internal = git_vector_get(&db->backends, i);
		git_odb_backend *b = internal->backend;

		/* we don't write in alternates! */
		if (internal->is_alternate)
			continue;

		if (b->write != NULL)
			error = b->write(oid, b, data, len, type);
	}

	if (!error || error == GIT_PASSTHROUGH)
		return 0;

	/* if no backends were able to write the object directly, we try a streaming
	 * write to the backends; just write the whole object into the stream in one
	 * push */

	if ((error = git_odb_open_wstream(&stream, db, len, type)) != 0)
		return error;

	stream->write(stream, data, len);
	error = stream->finalize_write(oid, stream);
	stream->free(stream);

	return error;
}

int git_odb_open_wstream(
	git_odb_stream **stream, git_odb *db, size_t size, git_otype type)
{
	size_t i;
	int error = GIT_ERROR;

	assert(stream && db);

	for (i = 0; i < db->backends.length && error < 0; ++i) {
		backend_internal *internal = git_vector_get(&db->backends, i);
		git_odb_backend *b = internal->backend;

		/* we don't write in alternates! */
		if (internal->is_alternate)
			continue;

		if (b->writestream != NULL)
			error = b->writestream(stream, b, size, type);
		else if (b->write != NULL)
			error = init_fake_wstream(stream, b, size, type);
	}

	if (error == GIT_PASSTHROUGH)
		error = 0;

	return error;
}

int git_odb_open_rstream(git_odb_stream **stream, git_odb *db, const git_oid *oid)
{
	size_t i;
	int error = GIT_ERROR;

	assert(stream && db);

	for (i = 0; i < db->backends.length && error < 0; ++i) {
		backend_internal *internal = git_vector_get(&db->backends, i);
		git_odb_backend *b = internal->backend;

		if (b->readstream != NULL)
			error = b->readstream(stream, b, oid);
	}

	if (error == GIT_PASSTHROUGH)
		error = 0;

	return error;
}

int git_odb_write_pack(struct git_odb_writepack **out, git_odb *db, git_transfer_progress_callback progress_cb, void *progress_payload)
{
	size_t i;
	int error = GIT_ERROR;

	assert(out && db);

	for (i = 0; i < db->backends.length && error < 0; ++i) {
		backend_internal *internal = git_vector_get(&db->backends, i);
		git_odb_backend *b = internal->backend;

		/* we don't write in alternates! */
		if (internal->is_alternate)
			continue;

		if (b->writepack != NULL)
			error = b->writepack(out, b, progress_cb, progress_payload);
	}

	if (error == GIT_PASSTHROUGH)
		error = 0;

	return error;
}

void *git_odb_backend_malloc(git_odb_backend *backend, size_t len)
{
	GIT_UNUSED(backend);
	return git__malloc(len);
}

int git_odb_refresh(struct git_odb *db)
{
	size_t i;
	assert(db);

	for (i = 0; i < db->backends.length; ++i) {
		backend_internal *internal = git_vector_get(&db->backends, i);
		git_odb_backend *b = internal->backend;

		if (b->refresh != NULL) {
			int error = b->refresh(b);
			if (error < 0)
				return error;
		}
	}

	return 0;
}

int git_odb__error_notfound(const char *message, const git_oid *oid)
{
	if (oid != NULL) {
		char oid_str[GIT_OID_HEXSZ + 1];
		git_oid_tostr(oid_str, sizeof(oid_str), oid);
		giterr_set(GITERR_ODB, "Object not found - %s (%s)", message, oid_str);
	} else
		giterr_set(GITERR_ODB, "Object not found - %s", message);

	return GIT_ENOTFOUND;
}

int git_odb__error_ambiguous(const char *message)
{
	giterr_set(GITERR_ODB, "Ambiguous SHA1 prefix - %s", message);
	return GIT_EAMBIGUOUS;
}

