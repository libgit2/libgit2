/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
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
	const char	*str;	/* type name string */
	int			loose; /* valid loose object type flag */
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
		GITERR_CHECK_ALLOC(object);
		memset(object, 0x0, git_object__size(type));
		break;

	default:
		giterr_set(GITERR_INVALID, "The given type is invalid");
		return -1;
	}

	object->type = type;

	*object_out = object;
	return 0;
}

int git_object__from_odb_object(
	git_object **object_out,
	git_repository *repo,
	git_odb_object *odb_obj,
	git_otype type)
{
	int error;
	git_object *object = NULL;

	if (type != GIT_OBJ_ANY && type != odb_obj->raw.type) {
		giterr_set(GITERR_INVALID, "The requested type does not match the type in the ODB");
		return GIT_ENOTFOUND;
	}

	type = odb_obj->raw.type;

	if ((error = create_object(&object, type)) < 0)
		return error;

	/* Initialize parent object */
	git_oid_cpy(&object->cached.oid, &odb_obj->cached.oid);
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

	if (error < 0)
		git_object__free(object);
	else
		*object_out = git_cache_try_store(&repo->objects, object);

	return error;
}

int git_object_lookup_prefix(
	git_object **object_out,
	git_repository *repo,
	const git_oid *id,
	size_t len,
	git_otype type)
{
	git_object *object = NULL;
	git_odb *odb = NULL;
	git_odb_object *odb_obj;
	int error = 0;

	assert(repo && object_out && id);

	if (len < GIT_OID_MINPREFIXLEN)
		return GIT_EAMBIGUOUS;

	error = git_repository_odb__weakptr(&odb, repo);
	if (error < 0)
		return error;

	if (len > GIT_OID_HEXSZ)
		len = GIT_OID_HEXSZ;

	if (len == GIT_OID_HEXSZ) {
		/* We want to match the full id : we can first look up in the cache,
		 * since there is no need to check for non ambiguousity
		 */
		object = git_cache_get(&repo->objects, id);
		if (object != NULL) {
			if (type != GIT_OBJ_ANY && type != object->type) {
				git_object_free(object);
				giterr_set(GITERR_INVALID, "The requested type does not match the type in ODB");
				return GIT_ENOTFOUND;
			}

			*object_out = object;
			return 0;
		}

		/* Object was not found in the cache, let's explore the backends.
		 * We could just use git_odb_read_unique_short_oid,
		 * it is the same cost for packed and loose object backends,
		 * but it may be much more costly for sqlite and hiredis.
		 */
		error = git_odb_read(&odb_obj, odb, id);
	} else {
		git_oid short_oid;

		/* We copy the first len*4 bits from id and fill the remaining with 0s */
		memcpy(short_oid.id, id->id, (len + 1) / 2);
		if (len % 2)
			short_oid.id[len / 2] &= 0xF0;
		memset(short_oid.id + (len + 1) / 2, 0, (GIT_OID_HEXSZ - len) / 2);

		/* If len < GIT_OID_HEXSZ (a strict short oid was given), we have
		 * 2 options :
		 * - We always search in the cache first. If we find that short oid is
		 *	ambiguous, we can stop. But in all the other cases, we must then
		 *	explore all the backends (to find an object if there was match,
		 *	or to check that oid is not ambiguous if we have found 1 match in
		 *	the cache)
		 * - We never explore the cache, go right to exploring the backends
		 * We chose the latter : we explore directly the backends.
		 */
		error = git_odb_read_prefix(&odb_obj, odb, &short_oid, len);
	}

	if (error < 0)
		return error;

	error = git_object__from_odb_object(object_out, repo, odb_obj, type);

	git_odb_object_free(odb_obj);

	return error;
}

int git_object_lookup(git_object **object_out, git_repository *repo, const git_oid *id, git_otype type) {
	return git_object_lookup_prefix(object_out, repo, id, GIT_OID_HEXSZ, type);
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
		git__free(object);
		break;
	}
}

void git_object_free(git_object *object)
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

static int dereference_object(git_object **dereferenced, git_object *obj)
{
	git_otype type = git_object_type(obj);

	switch (type) {
	case GIT_OBJ_COMMIT:
		return git_commit_tree((git_tree **)dereferenced, (git_commit*)obj);

	case GIT_OBJ_TAG:
		return git_tag_target(dereferenced, (git_tag*)obj);

	case GIT_OBJ_BLOB:
		return GIT_ENOTFOUND;

	case GIT_OBJ_TREE:
		return GIT_EAMBIGUOUS;

	default:
		return GIT_EINVALIDSPEC;
	}
}

static int peel_error(int error, const git_oid *oid, git_otype type)
{
	const char *type_name;
	char hex_oid[GIT_OID_HEXSZ + 1];

	type_name = git_object_type2string(type);

	git_oid_fmt(hex_oid, oid);
	hex_oid[GIT_OID_HEXSZ] = '\0';

	giterr_set(GITERR_OBJECT, "The git_object of id '%s' can not be "
		"successfully peeled into a %s (git_otype=%i).", hex_oid, type_name, type);

	return error;
}

int git_object_peel(
	git_object **peeled,
	const git_object *object,
	git_otype target_type)
{
	git_object *source, *deref = NULL;
	int error;

	if (target_type != GIT_OBJ_TAG && 
		target_type != GIT_OBJ_COMMIT && 
		target_type != GIT_OBJ_TREE && 
		target_type != GIT_OBJ_BLOB && 
		target_type != GIT_OBJ_ANY)
			return GIT_EINVALIDSPEC;

	assert(object && peeled);

	if (git_object_type(object) == target_type)
		return git_object__dup(peeled, (git_object *)object);

	source = (git_object *)object;

	while (!(error = dereference_object(&deref, source))) {

		if (source != object)
			git_object_free(source);

		if (git_object_type(deref) == target_type) {
			*peeled = deref;
			return 0;
		}

		if (target_type == GIT_OBJ_ANY &&
			git_object_type(deref) != git_object_type(object))
		{
			*peeled = deref;
			return 0;
		}

		source = deref;
		deref = NULL;
	}

	if (source != object)
		git_object_free(source);

	git_object_free(deref);

	if (error)
		error = peel_error(error, git_object_id(object), target_type);

	return error;
}

