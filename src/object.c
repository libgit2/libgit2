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
#include <stdarg.h>

#include "git2/object.h"

#include "common.h"
#include "repository.h"

#include "commit.h"
#include "tree.h"
#include "blob.h"
#include "tag.h"

static const int OBJECT_BASE_SIZE = 4096;

static struct {
	const char	*str;   /* type name string */
	int			loose;  /* valid loose object type flag */
	size_t		size;	/* size in bytes of the object structure */
} git_objects_table[] = {
	/* 0 = GIT_OBJ__EXT1 */
	{ "", 0, 0},

	/* 1 = GIT_OBJ_COMMIT */
	{ "commit", 1, sizeof(struct git_commit)},

	/* 2 = GIT_OBJ_TREE */
	{ "tree", 1, sizeof(struct git_tree) },

	/* 3 = GIT_OBJ_BLOB */
	{ "blob", 1, sizeof(struct git_blob) },

	/* 4 = GIT_OBJ_TAG */
	{ "tag", 1, sizeof(struct git_tag) },

	/* 5 = GIT_OBJ__EXT2 */
	{ "", 0, 0 },

	/* 6 = GIT_OBJ_OFS_DELTA */
	{ "OFS_DELTA", 0, 0 },

	/* 7 = GIT_OBJ_REF_DELTA */
	{ "REF_DELTA", 0, 0	}
};

/*
 * Object source methods
 *
 * Abstract buffer methods that allow the writeback system
 * to prepare the contents of any git file in-memory before
 * writing them to disk.
 */
static int source_resize(git_odb_source *src)
{
	size_t write_offset, new_size;
	void *new_data;

	write_offset = (size_t)((char *)src->write_ptr - (char *)src->raw.data);

	new_size = src->raw.len * 2;
	if ((new_data = git__malloc(new_size)) == NULL)
		return GIT_ENOMEM;

	memcpy(new_data, src->raw.data, src->written_bytes);
	free(src->raw.data);

	src->raw.data = new_data;
	src->raw.len = new_size;
	src->write_ptr = (char *)new_data + write_offset;

	return GIT_SUCCESS;
}

int git__source_printf(git_odb_source *source, const char *format, ...)
{
	va_list arglist;
	int len;

	assert(source->open && source->write_ptr);

	va_start(arglist, format);

	len = vsnprintf(source->write_ptr, source->raw.len - source->written_bytes, format, arglist);

	while (source->written_bytes + len >= source->raw.len) {
		if (source_resize(source) < GIT_SUCCESS)
			return GIT_ENOMEM;

		len = vsnprintf(source->write_ptr, source->raw.len - source->written_bytes, format, arglist);
	}

	source->write_ptr = (char *)source->write_ptr + len;
	source->written_bytes += len;

	return GIT_SUCCESS;
}

int git__source_write(git_odb_source *source, const void *bytes, size_t len)
{
	assert(source);

	assert(source->open && source->write_ptr);

	while (source->written_bytes + len >= source->raw.len) {
		if (source_resize(source) < GIT_SUCCESS)
			return GIT_ENOMEM;
	}

	memcpy(source->write_ptr, bytes, len);
	source->write_ptr = (char *)source->write_ptr + len;
	source->written_bytes += len;

	return GIT_SUCCESS;
}

static void prepare_write(git_object *object)
{
	if (object->source.write_ptr != NULL || object->source.open)
		git_object__source_close(object);

	/* TODO: proper size calculation */
	object->source.raw.data = git__malloc(OBJECT_BASE_SIZE);
	object->source.raw.len = OBJECT_BASE_SIZE;

	object->source.write_ptr = object->source.raw.data;
	object->source.written_bytes = 0;

	object->source.open = 1;
}

static int write_back(git_object *object)
{
	int error;
	git_oid new_id;

	assert(object);

	assert(object->source.open);
	assert(object->modified);

	object->source.raw.len = object->source.written_bytes;

	if ((error = git_odb_write(&new_id, object->repo->db, &object->source.raw)) < GIT_SUCCESS)
		return error;

	if (object->in_memory) {
		int idx = git_vector_search(&object->repo->memory_objects, object);
		git_vector_remove(&object->repo->memory_objects, idx);
	} else {
		git_hashtable_remove(object->repo->objects, &object->id);
	}

	git_oid_cpy(&object->id, &new_id);
	git_hashtable_insert(object->repo->objects, &object->id, object);

	object->source.write_ptr = NULL;
	object->source.written_bytes = 0;

	object->modified = 0;
	object->in_memory = 0;

	git_object__source_close(object);
	return GIT_SUCCESS;
}

int git_object__source_open(git_object *object)
{
	int error;

	assert(object && !object->in_memory);

	if (object->source.open)
		git_object__source_close(object);

	error = git_odb_read(&object->source.raw, object->repo->db, &object->id);
	if (error < GIT_SUCCESS)
		return error;

	object->source.open = 1;
	return GIT_SUCCESS;
}

void git_object__source_close(git_object *object)
{
	assert(object);

	if (object->source.open) {
		git_rawobj_close(&object->source.raw);
		object->source.open = 0;
	}
}

static int create_object(git_object **object_out, git_otype type)
{
	git_object *object = NULL;

	assert(object_out);

	*object_out = NULL;

	switch (type) {
	case GIT_OBJ_COMMIT:
	case GIT_OBJ_TAG:
	case GIT_OBJ_BLOB:
		object = git__malloc(git_object__size(type));
		if (object == NULL)
			return GIT_ENOMEM;
		memset(object, 0x0, git_object__size(type));
		break;
		
	case GIT_OBJ_TREE:
		object = (git_object *)git_tree__new();
		if (object == NULL)
			return GIT_ENOMEM;
		break;

	default:
		return GIT_EINVALIDTYPE;
	}

	*object_out = object;
	return GIT_SUCCESS;
}

int git_object_new(git_object **object_out, git_repository *repo, git_otype type)
{
	git_object *object = NULL;
	int error;

	assert(object_out && repo);

	if ((error = create_object(&object, type)) < GIT_SUCCESS)
		return error;

	object->repo = repo;
	object->in_memory = 1;
	object->modified = 1;

	object->source.raw.type = type;

	object->refcount++;
	*object_out = object;
	return GIT_SUCCESS;
}

int git_object_lookup(git_object **object_out, git_repository *repo, const git_oid *id, git_otype type)
{
	git_object *object = NULL;
	git_rawobj obj_file;
	int error = GIT_SUCCESS;

	assert(repo && object_out && id);

	object = git_hashtable_lookup(repo->objects, id);
	if (object != NULL) {
		*object_out = object;
		GIT_OBJECT_INCREF(object);
		return GIT_SUCCESS;
	}

	error = git_odb_read(&obj_file, repo->db, id);
	if (error < GIT_SUCCESS)
		return error;

	if (type != GIT_OBJ_ANY && type != obj_file.type) {
		git_rawobj_close(&obj_file);
		return GIT_EINVALIDTYPE;
	}

	type = obj_file.type;

	if ((error = create_object(&object, type)) < GIT_SUCCESS)
		return error;

	/* Initialize parent object */
	git_oid_cpy(&object->id, id);
	object->repo = repo;
	memcpy(&object->source.raw, &obj_file, sizeof(git_rawobj));
	object->source.open = 1;

	switch (type) {
	case GIT_OBJ_COMMIT:
		error = git_commit__parse((git_commit *)object);
		break;

	case GIT_OBJ_TREE:
		error = git_tree__parse((git_tree *)object);
		break;

	case GIT_OBJ_TAG:
		error = git_tag__parse((git_tag *)object);
		break;

	case GIT_OBJ_BLOB:
		error = git_blob__parse((git_blob *)object);
		break;

	default:
		break;
	}

	if (error < GIT_SUCCESS) {
		git_object__free(object);
		return error;
	}

	git_object__source_close(object);
	git_hashtable_insert(repo->objects, &object->id, object);

	GIT_OBJECT_INCREF(object);
	*object_out = object;
	return GIT_SUCCESS;
}

int git_object_write(git_object *object)
{
	int error;
	git_odb_source *source;

	assert(object);

	if (object->modified == 0)
		return GIT_SUCCESS;

	prepare_write(object);
	source = &object->source;

	switch (source->raw.type) {
	case GIT_OBJ_COMMIT:
		error = git_commit__writeback((git_commit *)object, source);
		break;

	case GIT_OBJ_TREE:
		error = git_tree__writeback((git_tree *)object, source);
		break;

	case GIT_OBJ_TAG:
		error = git_tag__writeback((git_tag *)object, source);
		break;

	case GIT_OBJ_BLOB:
		error = git_blob__writeback((git_blob *)object, source);
		break;

	default:
		error = GIT_ERROR;
		break;
	}

	if (error < GIT_SUCCESS) {
		git_object__source_close(object);
		return error;
	}

	return write_back(object);
}

void git_object__free(git_object *object)
{
	assert(object);

	git_object__source_close(object);

	if (object->repo != NULL) {
		if (object->in_memory) {
			int idx = git_vector_search(&object->repo->memory_objects, object);
			git_vector_remove(&object->repo->memory_objects, idx);
		} else {
			git_hashtable_remove(object->repo->objects, &object->id);
		}
	}

	switch (object->source.raw.type) {
	case GIT_OBJ_COMMIT:
		git_commit__free((git_commit *)object);
		break;

	case GIT_OBJ_TREE:
		git_tree__free((git_tree *)object);
		break;

	case GIT_OBJ_TAG:
		git_tag__free((git_tag *)object);
		break;

	case GIT_OBJ_BLOB:
		git_blob__free((git_blob *)object);
		break;

	default:
		free(object);
		break;
	}
}

void git_object_close(git_object *object)
{
	if (object == NULL)
		return;

	if (--object->refcount <= 0)
		git_object__free(object);
}

const git_oid *git_object_id(const git_object *obj)
{
	assert(obj);

	if (obj->in_memory)
		return NULL;

	return &obj->id;
}

git_otype git_object_type(const git_object *obj)
{
	assert(obj);
	return obj->source.raw.type;
}

git_repository *git_object_owner(const git_object *obj)
{
	assert(obj);
	return obj->repo;
}

const char *git_object_type2string(git_otype type)
{
	if (type < 0 || ((size_t) type) >= ARRAY_SIZE(git_objects_table))
		return "";

	return git_objects_table[type].str;
}

git_otype git_object_string2type(const char *str)
{
	size_t i;

	if (!str || !*str)
		return GIT_OBJ_BAD;

	for (i = 0; i < ARRAY_SIZE(git_objects_table); i++)
		if (!strcmp(str, git_objects_table[i].str))
			return (git_otype)i;

	return GIT_OBJ_BAD;
}

int git_object_typeisloose(git_otype type)
{
	if (type < 0 || ((size_t) type) >= ARRAY_SIZE(git_objects_table))
		return 0;

	return git_objects_table[type].loose;
}

size_t git_object__size(git_otype type)
{
	if (type < 0 || ((size_t) type) >= ARRAY_SIZE(git_objects_table))
		return 0;

	return git_objects_table[type].size;
}

