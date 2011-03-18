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

static int create_object(git_object **object_out, git_otype type)
{
	git_object *object = NULL;

	assert(object_out);

	*object_out = NULL;

	switch (type) {
	case GIT_OBJ_COMMIT:
	case GIT_OBJ_TAG:
	case GIT_OBJ_BLOB:
	case GIT_OBJ_TREE:
		object = git__malloc(git_object__size(type));
		if (object == NULL)
			return GIT_ENOMEM;
		memset(object, 0x0, git_object__size(type));
		break;

	default:
		return GIT_EINVALIDTYPE;
	}

	object->type = type;

	*object_out = object;
	return GIT_SUCCESS;
}

int git_object_lookup(git_object **object_out, git_repository *repo, const git_oid *id, git_otype type)
{
	git_object *object = NULL;
	git_odb_object *odb_obj;
	int error = GIT_SUCCESS;

	assert(repo && object_out && id);

	object = git_cache_get(&repo->objects, id);
	if (object != NULL) {
		if (type != GIT_OBJ_ANY && type != object->type)
			return GIT_EINVALIDTYPE;

		*object_out = object;
		return GIT_SUCCESS;
	}

	error = git_odb_read(&odb_obj, repo->db, id);
	if (error < GIT_SUCCESS)
		return error;

	if (type != GIT_OBJ_ANY && type != odb_obj->raw.type) {
		git_odb_object_close(odb_obj);
		return GIT_EINVALIDTYPE;
	}

	type = odb_obj->raw.type;

	if ((error = create_object(&object, type)) < GIT_SUCCESS)
		return error;

	/* Initialize parent object */
	git_oid_cpy(&object->cached.oid, id);
	object->repo = repo;

	switch (type) {
	case GIT_OBJ_COMMIT:
		error = git_commit__parse((git_commit *)object, odb_obj);
		break;

	case GIT_OBJ_TREE:
		error = git_tree__parse((git_tree *)object, odb_obj);
		break;

	case GIT_OBJ_TAG:
		error = git_tag__parse((git_tag *)object, odb_obj);
		break;

	case GIT_OBJ_BLOB:
		error = git_blob__parse((git_blob *)object, odb_obj);
		break;

	default:
		break;
	}

	git_odb_object_close(odb_obj);

	if (error < GIT_SUCCESS) {
		git_object__free(object);
		return error;
	}

	*object_out = git_cache_try_store(&repo->objects, object);
	return GIT_SUCCESS;
}

void git_object__free(void *_obj)
{
	git_object *object = (git_object *)_obj;

	assert(object);

	switch (object->type) {
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

	git_cached_obj_decref((git_cached_obj *)object, git_object__free);
}

const git_oid *git_object_id(const git_object *obj)
{
	assert(obj);
	return &obj->cached.oid;
}

git_otype git_object_type(const git_object *obj)
{
	assert(obj);
	return obj->type;
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

