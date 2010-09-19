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
#include "repository.h"
#include "commit.h"
#include "tag.h"

static const int default_table_size = 32;
static const double max_load_factor = 0.65;

uint32_t git_object_hash(const void *key)
{
	uint32_t r;
	git_oid *id;

	id = (git_oid *)key;
	memcpy(&r, id->id, sizeof(r));
	return r;
}

int git_object_haskey(void *object, const void *key)
{
	git_object *obj;
	git_oid *oid;

	obj = (git_object *)object;
	oid = (git_oid *)key;

	return (git_oid_cmp(oid, &obj->id) == 0);
}

git_repository *git_repository_alloc(git_odb *odb)
{
	git_repository *repo = git__malloc(sizeof(git_repository));
	if (!repo)
		return NULL;

	memset(repo, 0x0, sizeof(git_repository));

	repo->objects = git_hashtable_alloc(
			default_table_size, 
			git_object_hash,
			git_object_haskey);

	if (repo->objects == NULL) {
		free(repo);
		return NULL;
	}

	repo->db = odb; /* TODO: create ODB manually! */

	return repo;
}

void git_repository_free(git_repository *repo)
{
	git_hashtable_iterator it;
	git_object *object;

	git_hashtable_iterator_init(repo->objects, &it);

	while ((object = (git_object *)
				git_hashtable_iterator_next(&it)) != NULL)
		git_object_free(object);

	git_hashtable_free(repo->objects);
	/* TODO: free odb */
	free(repo);
}

void git_object__source_prepare_write(git_object *object)
{
	size_t base_size = 512;

	if (object->source.write_ptr != NULL || object->source.open)
		git_object__source_close(object);

	/* TODO: proper size calculation */
	object->source.raw.data = git__malloc(base_size);
	object->source.raw.len = base_size;

	object->source.write_ptr = object->source.raw.data;
	object->source.written_bytes = 0;

	object->source.open = 1;
	object->source.out_of_sync = 1;
}

int git_object__source_write(git_object *object, const void *bytes, size_t len)
{
	assert(object);

	if (!object->source.open || object->source.write_ptr == NULL)
		return GIT_ERROR;

	/* TODO: resize buffer on overflow */
	if (object->source.written_bytes + len >= object->source.raw.len)
		return GIT_ENOMEM;

	memcpy(object->source.write_ptr, bytes, len);
	object->source.write_ptr += len;
	object->source.written_bytes += len;

	return GIT_SUCCESS;
}

int git_object__source_writeback(git_object *object)
{
	int error;
	git_oid new_id;

	assert(object);

	if (!object->source.open)
		return GIT_ERROR;

	if (!object->source.out_of_sync)
		return GIT_SUCCESS;
	
	object->source.raw.len = object->source.written_bytes;

	git_obj_hash(&new_id, &object->source.raw);

	if ((error = git_odb_write(&new_id, object->repo->db, &object->source.raw)) < 0)
		return error;

	git_hashtable_remove(object->repo->objects, &object->id);
	git_oid_cpy(&object->id, &new_id);
	git_hashtable_insert(object->repo->objects, &object->id, object);

	object->source.write_ptr = NULL;
	object->source.written_bytes = 0;

	git_object__source_close(object);
	return GIT_SUCCESS;
}

int git_object__source_open(git_object *object)
{
	int error;

	assert(object);

	if (object->source.open && object->source.out_of_sync)
		git_object__source_close(object);

	if (object->source.open)
		return GIT_SUCCESS;

	error = git_odb_read(&object->source.raw, object->repo->db, &object->id);
	if (error < 0)
		return error;

	object->source.open = 1;
	object->source.out_of_sync = 0;
	return GIT_SUCCESS;
}

void git_object__source_close(git_object *object)
{
	assert(object);

	if (!object->source.open) {
		git_obj_close(&object->source.raw);
		object->source.open = 0;
		object->source.out_of_sync = 0;
	}
}

void git_object_free(git_object *object)
{
	assert(object);

	git_hashtable_remove(object->repo->objects, &object->id);
	git_obj_close(&object->source.raw);

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

	default:
		free(object);
		break;
	}
}

git_odb *git_repository_database(git_repository *repo)
{
	assert(repo);
	return repo->db;
}

const git_oid *git_object_id(git_object *obj)
{
	assert(obj);
	return &obj->id;
}

git_otype git_object_type(git_object *obj)
{
	assert(obj);
	return obj->source.raw.type;
}

git_object *git_repository_lookup(git_repository *repo, const git_oid *id, git_otype type)
{
	static const size_t object_sizes[] = {
		0,
		sizeof(git_commit),
		sizeof(git_tree),
		sizeof(git_object), /* TODO: sizeof(git_blob) */ 
		sizeof(git_tag)
	};

	git_object *object = NULL;
	git_rawobj obj_file;

	assert(repo);

	object = git_hashtable_lookup(repo->objects, id);
	if (object != NULL)
		return object;

	if (git_odb_read(&obj_file, repo->db, id) < 0)
		return NULL;

	if (type != GIT_OBJ_ANY && type != obj_file.type)
		return NULL;

	type = obj_file.type;

	object = git__malloc(object_sizes[type]);

	if (object == NULL)
		return NULL;

	memset(object, 0x0, object_sizes[type]);

	/* Initialize parent object */
	git_oid_cpy(&object->id, id);
	object->repo = repo;
	object->source.open = 1;
	memcpy(&object->source.raw, &obj_file, sizeof(git_rawobj));

	switch (type) {

	case GIT_OBJ_COMMIT:
		if (git_commit__parse_basic((git_commit *)object) < 0) {
			free(object);
			return NULL;
		}

		break;

	case GIT_OBJ_TREE:
		if (git_tree__parse((git_tree *)object) < 0) {
			free(object);
			return NULL;
		}

		break;

	case GIT_OBJ_TAG:
		if (git_tag__parse((git_tag *)object) < 0) {
			free(object);
			return NULL;
		}

		break;

	default:
		/* blobs get no parsing */
		break;
	}

	git_obj_close(&object->source.raw);
	object->source.open = 0;

	git_hashtable_insert(repo->objects, &object->id, object);
	return object;
}
