/*
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2,
 * as published by the Free Software Foundation.
 *
 * In addition to the permissions in the GNU General Public License,
 * the authors give you unlimited permission to link the compiled
 * version of this file into combinations with other programs,
 * and to distribute those combinations without any restriction
 * coming from the use of this file.  (The General Public License
 * restrictions do apply in other respects; for example, they cover
 * modification of the file, and distribution when not linked into
 * a combined executable.)
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "common.h"
#include "git2/zlib.h"
#include "git2/object.h"
#include "fileops.h"
#include "hash.h"
#include "odb.h"
#include "delta-apply.h"

#include "git2/odb_backend.h"

#define GIT_ALTERNATES_FILE "info/alternates"

/* TODO: is this correct? */
#define GIT_LOOSE_PRIORITY 2
#define GIT_PACKED_PRIORITY 1

typedef struct
{
	git_odb_backend *backend;
	int priority;
	int is_alternate;
} backend_internal;

static int format_object_header(char *hdr, size_t n, git_rawobj *obj)
{
	const char *type_str = git_object_type2string(obj->type);
	int len = snprintf(hdr, n, "%s %"PRIuZ, type_str, obj->len);

	assert(len > 0);             /* otherwise snprintf() is broken  */
	assert(((size_t) len) < n);  /* otherwise the caller is broken! */

	if (len < 0 || ((size_t) len) >= n)
		return GIT_ERROR;
	return len+1;
}

int git_odb__hash_obj(git_oid *id, char *hdr, size_t n, int *len, git_rawobj *obj)
{
	git_buf_vec vec[2];
	int  hdrlen;

	assert(id && hdr && len && obj);

	if (!git_object_typeisloose(obj->type))
		return GIT_ERROR;

	if (!obj->data && obj->len != 0)
		return GIT_ERROR;

	if ((hdrlen = format_object_header(hdr, n, obj)) < 0)
		return GIT_ERROR;

	*len = hdrlen;

	vec[0].data = hdr;
	vec[0].len  = hdrlen;
	vec[1].data = obj->data;
	vec[1].len  = obj->len;

	git_hash_vec(id, vec, 2);

	return GIT_SUCCESS;
}

void git_rawobj_close(git_rawobj *obj)
{
	free(obj->data);
	obj->data = NULL;
}

int git_rawobj_hash(git_oid *id, git_rawobj *obj)
{
	char hdr[64];
	int  hdrlen;

	assert(id && obj);

	return git_odb__hash_obj(id, hdr, sizeof(hdr), &hdrlen, obj);
}

int git_odb__inflate_buffer(void *in, size_t inlen, void *out, size_t outlen)
{
	z_stream zs;
	int status = Z_OK;

	memset(&zs, 0x0, sizeof(zs));

	zs.next_out  = out;
	zs.avail_out = outlen;

	zs.next_in  = in;
	zs.avail_in = inlen;

	if (inflateInit(&zs) < Z_OK)
		return GIT_ERROR;

	while (status == Z_OK)
		status = inflate(&zs, Z_FINISH);

	inflateEnd(&zs);

	if ((status != Z_STREAM_END) /*|| (zs.avail_in != 0) */)
		return GIT_ERROR;

	if (zs.total_out != outlen)
		return GIT_ERROR;

	return GIT_SUCCESS;
}





/***********************************************************
 *
 * OBJECT DATABASE PUBLIC API
 *
 * Public calls for the ODB functionality
 *
 ***********************************************************/

int backend_sort_cmp(const void *a, const void *b)
{
	const backend_internal *backend_a = *(const backend_internal **)(a);
	const backend_internal *backend_b = *(const backend_internal **)(b);

	if (backend_a->is_alternate == backend_b->is_alternate)
		return (backend_b->priority - backend_a->priority);

	return backend_a->is_alternate ? 1 : -1;
}

int git_odb_new(git_odb **out)
{
	git_odb *db = git__calloc(1, sizeof(*db));
	if (!db)
		return GIT_ENOMEM;

	if (git_vector_init(&db->backends, 4, backend_sort_cmp, NULL) < 0) {
		free(db);
		return GIT_ENOMEM;
	}

	*out = db;
	return GIT_SUCCESS;
}

static int add_backend_internal(git_odb *odb, git_odb_backend *backend, int priority, int is_alternate)
{
	backend_internal *internal;

	assert(odb && backend);

	if (backend->odb != NULL && backend->odb != odb)
		return GIT_EBUSY;

	internal = git__malloc(sizeof(backend_internal));
	if (internal == NULL)
		return GIT_ENOMEM;

	internal->backend = backend;
	internal->priority = priority;
	internal->is_alternate = is_alternate;

	if (git_vector_insert(&odb->backends, internal) < 0) {
		free(internal);
		return GIT_ENOMEM;
	}

	git_vector_sort(&odb->backends);
	internal->backend->odb = odb;
	return GIT_SUCCESS;
}

int git_odb_add_backend(git_odb *odb, git_odb_backend *backend, int priority)
{
	return add_backend_internal(odb, backend, priority, 0);
}

int git_odb_add_alternate(git_odb *odb, git_odb_backend *backend, int priority)
{
	return add_backend_internal(odb, backend, priority, 1);
}

static int add_default_backends(git_odb *db, const char *objects_dir, int as_alternates)
{
	git_odb_backend *loose, *packed;
	int error;

	/* add the loose object backend */
	error = git_odb_backend_loose(&loose, objects_dir);
	if (error < GIT_SUCCESS)
		return error;

	error = add_backend_internal(db, loose, GIT_LOOSE_PRIORITY, as_alternates);
	if (error < GIT_SUCCESS)
		return error;

	/* add the packed file backend */
	error = git_odb_backend_pack(&packed, objects_dir);
	if (error < GIT_SUCCESS)
		return error;

	error = add_backend_internal(db, packed, GIT_PACKED_PRIORITY, as_alternates);
	if (error < GIT_SUCCESS)
		return error;

	return GIT_SUCCESS;
}

static int load_alternates(git_odb *odb, const char *objects_dir)
{
	char alternates_path[GIT_PATH_MAX];
	char alternate[GIT_PATH_MAX];
	char *buffer;

	gitfo_buf alternates_buf = GITFO_BUF_INIT;
	int error;

	git__joinpath(alternates_path, objects_dir, GIT_ALTERNATES_FILE);

	if (gitfo_exists(alternates_path) < GIT_SUCCESS)
		return GIT_SUCCESS;

	if (gitfo_read_file(&alternates_buf, alternates_path) < GIT_SUCCESS)
		return GIT_EOSERR;

	buffer = (char *)alternates_buf.data;
	error = GIT_SUCCESS;

	/* add each alternate as a new backend; one alternate per line */
	while ((error == GIT_SUCCESS) && (buffer = git__strtok(alternate, buffer, "\r\n")) != NULL)
		error = add_default_backends(odb, alternate, 1);

	gitfo_free_buf(&alternates_buf);
	return error;
}

int git_odb_open(git_odb **out, const char *objects_dir)
{
	git_odb *db;
	int error;

	assert(out && objects_dir);

	*out = NULL;

	if ((error = git_odb_new(&db)) < 0)
		return error;

	if ((error = add_default_backends(db, objects_dir, 0)) < GIT_SUCCESS)
		goto cleanup;

	if ((error = load_alternates(db, objects_dir)) < GIT_SUCCESS)
		goto cleanup;

	*out = db;
	return GIT_SUCCESS;

cleanup:
	git_odb_close(db);
	return error;
}

void git_odb_close(git_odb *db)
{
	unsigned int i;

	if (db == NULL)
		return;

	for (i = 0; i < db->backends.length; ++i) {
		backend_internal *internal = git_vector_get(&db->backends, i);
		git_odb_backend *backend = internal->backend;

		if (backend->free) backend->free(backend);
		else free(backend);

		free(internal);
	}

	git_vector_free(&db->backends);
	free(db);
}

int git_odb_exists(git_odb *db, const git_oid *id)
{
	unsigned int i;
	int found = 0;

	assert(db && id);

	for (i = 0; i < db->backends.length && !found; ++i) {
		backend_internal *internal = git_vector_get(&db->backends, i);
		git_odb_backend *b = internal->backend;

		if (b->exists != NULL)
			found = b->exists(b, id);
	}

	return found;
}

int git_odb_read_header(git_rawobj *out, git_odb *db, const git_oid *id)
{
	unsigned int i;
	int error = GIT_ENOTFOUND;

	assert(out && db && id);

	for (i = 0; i < db->backends.length && error < 0; ++i) {
		backend_internal *internal = git_vector_get(&db->backends, i);
		git_odb_backend *b = internal->backend;

		if (b->read_header != NULL)
			error = b->read_header(out, b, id);
	}

	/*
	 * no backend could read only the header.
	 * try reading the whole object and freeing the contents
	 */
	if (error < 0) {
		error = git_odb_read(out, db, id);
		git_rawobj_close(out);
	}

	return error;
}

int git_odb_read(git_rawobj *out, git_odb *db, const git_oid *id)
{
	unsigned int i;
	int error = GIT_ENOTFOUND;

	assert(out && db && id);

	for (i = 0; i < db->backends.length && error < 0; ++i) {
		backend_internal *internal = git_vector_get(&db->backends, i);
		git_odb_backend *b = internal->backend;

		if (b->read != NULL)
			error = b->read(out, b, id);
	}

	return error;
}

int git_odb_write(git_oid *id, git_odb *db, git_rawobj *obj)
{
	unsigned int i;
	int error = GIT_ERROR;

	assert(obj && db && id);

	for (i = 0; i < db->backends.length && error < 0; ++i) {
		backend_internal *internal = git_vector_get(&db->backends, i);
		git_odb_backend *b = internal->backend;

		/* we don't write in alternates! */
		if (internal->is_alternate)
			continue;

		if (b->write != NULL)
			error = b->write(id, b, obj);
	}

	return error;
}

