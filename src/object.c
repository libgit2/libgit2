/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#include "git2/object.h"

#include "common.h"
#include "repository.h"

#include "commit.h"
#include "tree.h"
#include "blob.h"
#include "tag.h"
#include "signature.h"

static const int OBJECT_BASE_SIZE = 4096;

typedef struct {
	const char	*str;	/* type name string */
	size_t		size;	/* size in bytes of the object structure */

	int  (*parse)(void *self, git_odb_object *obj);
	void (*free)(void *self);
} git_object_def;

static git_object_def git_objects_table[] = {
	/* 0 = GIT_OBJ__EXT1 */
	{ "", 0, NULL, NULL },

	/* 1 = GIT_OBJ_COMMIT */
	{ "commit", sizeof(git_commit), git_commit__parse, git_commit__free },

	/* 2 = GIT_OBJ_TREE */
	{ "tree", sizeof(git_tree), git_tree__parse, git_tree__free },

	/* 3 = GIT_OBJ_BLOB */
	{ "blob", sizeof(git_blob), git_blob__parse, git_blob__free },

	/* 4 = GIT_OBJ_TAG */
	{ "tag", sizeof(git_tag), git_tag__parse, git_tag__free },

	/* 5 = GIT_OBJ__EXT2 */
	{ "", 0, NULL, NULL },
	/* 6 = GIT_OBJ_OFS_DELTA */
	{ "OFS_DELTA", 0, NULL, NULL },
	/* 7 = GIT_OBJ_REF_DELTA */
	{ "REF_DELTA", 0, NULL, NULL },
};

static int git_object__match_cache(git_otype type, git_otype cached)
{
	if (type == GIT_OBJ_ANY || type == cached)
		return 0;

	giterr_set(
		GITERR_INVALID,
		"Requested object type (%s) does not match type in ODB (%s)",
		git_object_type2string(type), git_object_type2string(cached));
	return GIT_ENOTFOUND;
}

int git_object__from_odb_object(
	git_object **out,
	git_repository *repo,
	git_odb_object *odb_obj,
	git_otype type,
	bool lax)
{
	int error;
	size_t object_size;
	git_object_def *def;
	git_object *object = NULL;

	assert(out);
	*out = NULL;

	/* Validate type match */
	if ((error = git_object__match_cache(type, odb_obj->cached.type)) < 0)
		return error;

	if ((object_size = git_object__size(odb_obj->cached.type)) == 0) {
		giterr_set(GITERR_INVALID, "The requested type is invalid");
		return GIT_ENOTFOUND;
	}

	/* Allocate and initialize base object */
	object = git__calloc(1, object_size);
	GITERR_CHECK_ALLOC(object);

	git_oid_cpy(&object->cached.oid, &odb_obj->cached.oid);
	object->cached.type = odb_obj->cached.type;
	object->cached.size = odb_obj->cached.size;
	object->repo = repo;

	/* Parse raw object data */
	def = &git_objects_table[odb_obj->cached.type];
	assert(def->free && def->parse);

	if ((error = def->parse(object, odb_obj)) < 0) {
		if (lax) /* do not put invalid objects into cache */
			*out = object;
		else
			def->free(object);
	} else {
		*out = git_cache_store_parsed(&repo->objects, object);
	}

	return error;
}

void git_object__free(void *obj)
{
	git_otype type = ((git_object *)obj)->cached.type;

	if (type < 0 || ((size_t)type) >= ARRAY_SIZE(git_objects_table) ||
		!git_objects_table[type].free)
		git__free(obj);
	else
		git_objects_table[type].free(obj);
}

static int object_lookup(
	git_object **out,
	git_repository *repo,
	const git_oid *id,
	size_t len,
	git_otype type,
	bool lax)
{
	int error = 0;
	git_odb *odb = NULL;
	git_odb_object *odb_obj = NULL;

	assert(repo && out && id);

	if (len < GIT_OID_MINPREFIXLEN) {
		giterr_set(GITERR_OBJECT,
			"Ambiguous lookup - OID prefix is too short (%d)", (int)len);
		return GIT_EAMBIGUOUS;
	}

	if (type != GIT_OBJ_ANY && !git_object__size(type)) {
		giterr_set(
			GITERR_INVALID, "The requested type (%d) is invalid", (int)type);
		return GIT_ENOTFOUND;
	}

	if ((error = git_repository_odb__weakptr(&odb, repo)) < 0)
		return error;

	if (len > GIT_OID_HEXSZ)
		len = GIT_OID_HEXSZ;

	if (len == GIT_OID_HEXSZ) {
		git_cached_obj *cached = NULL;

		/* Full id: first look in cache, since there is no ambiguity */
		cached = git_cache_get_any(&repo->objects, id);

		if (!cached)
			/* Object not found in cache, so search backends */
			error = git_odb_read(&odb_obj, odb, id);
		else if (cached->flags == GIT_CACHE_STORE_PARSED) {
			if ((error = git_object__match_cache(type, cached->type)) < 0)
				git_object_free((git_object *)cached);
			else
				*out = (git_object *)cached;
			return error;
		}
		else if (cached->flags == GIT_CACHE_STORE_RAW)
			odb_obj = (git_odb_object *)cached;
		else
			assert(!"Wrong caching type in the global object cache");
	} else {
		git_oid short_oid = {{0}};

		/* Copy first len*4 bits from id and fill the remaining with 0s */
		memcpy(short_oid.id, id->id, (len + 1) / 2);
		if (len % 2)
			short_oid.id[len / 2] &= 0xF0;

		/* If len < GIT_OID_HEXSZ (short oid), we have 2 options:
		 *
		 * - We always search in the cache first. If we find that short
		 *	 oid is ambiguous, we can stop. But in all the other cases, we
		 *	 must then explore all the backends (to find an object if
		 *	 there was match, or to check that oid is not ambiguous if we
		 *	 have found 1 match in the cache)
		 *
		 * - We never explore the cache, go right to exploring the
		 *   backends We chose the latter : we explore directly the
		 *   backends.
		 */
		error = git_odb_read_prefix(&odb_obj, odb, &short_oid, len);
	}

	if (!error) {
		error = git_object__from_odb_object(out, repo, odb_obj, type, lax);

		git_odb_object_free(odb_obj);
	}

	return error;
}

int git_object_lookup(
	git_object **out,
	git_repository *repo,
	const git_oid *id,
	git_otype type)
{
	return object_lookup(out, repo, id, GIT_OID_HEXSZ, type, false);
}

int git_object_lookup_prefix(
	git_object **out,
	git_repository *repo,
	const git_oid *id,
	size_t len,
	git_otype type)
{
	return object_lookup(out, repo, id, len, type, false);
}

int git_object_lookup_lax(
	git_object **out,
	git_repository *repo,
	const git_oid *id,
	size_t len,
	git_otype type)
{
	return object_lookup(out, repo, id, len, type, true);
}

void git_object_free(git_object *object)
{
	if (object == NULL)
		return;
	git_cached_obj_decref(object);
}

const git_oid *git_object_id(const git_object *obj)
{
	assert(obj);
	return &obj->cached.oid;
}

git_otype git_object_type(const git_object *obj)
{
	assert(obj);
	return obj->cached.type;
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

git_otype git_object_string2type(const char *str, size_t len)
{
	size_t i;

	if (!str || !*str)
		return GIT_OBJ_BAD;
	if (!len)
		len = strlen(str);

	for (i = 0; i < ARRAY_SIZE(git_objects_table); i++) {
		size_t typelen = strlen(git_objects_table[i].str);

		if (len >= typelen && !memcmp(str, git_objects_table[i].str, len))
			return (git_otype)i;
	}

	return GIT_OBJ_BAD;
}

int git_object_typeisloose(git_otype type)
{
	if (type < 0 || ((size_t) type) >= ARRAY_SIZE(git_objects_table))
		return 0;

	return (git_objects_table[type].size > 0) ? 1 : 0;
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

	assert(object && peeled);

	if (git_object_type(object) == target_type)
		return git_object_dup(peeled, (git_object *)object);

	assert(target_type == GIT_OBJ_TAG ||
		target_type == GIT_OBJ_COMMIT ||
		target_type == GIT_OBJ_TREE ||
		target_type == GIT_OBJ_BLOB ||
		target_type == GIT_OBJ_ANY);

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

int git_object_dup(git_object **dest, git_object *source)
{
	git_cached_obj_incref(source);
	*dest = source;
	return 0;
}

int git_object_lookup_bypath(
	git_object **out,
	const git_object *treeish,
	const char *path,
	git_otype type)
{
	int error = 0;
	git_object *tree = NULL;
	git_tree_entry *entry = NULL;

	assert(out && treeish && path);

	if ((error = git_object_peel(&tree, treeish, GIT_OBJ_TREE)) < 0 ||
		(error = git_tree_entry_bypath(&entry, (git_tree *)tree, path)) < 0)
		goto cleanup;

	if (type != GIT_OBJ_ANY && git_tree_entry_type(entry) != type) {
		giterr_set(
			GITERR_OBJECT, "object at path '%s' is not a %s (%d)",
			path, git_object_type2string(type), type);
		error = GIT_EINVALIDSPEC;
		goto cleanup;
	}

	error = git_tree_entry_to_object(out, git_object_owner(treeish), entry);

cleanup:
	git_tree_entry_free(entry);
	git_object_free(tree);

	return error;
}

int git_object_short_id(git_buf *out, const git_object *obj)
{
	git_repository *repo;
	int len = GIT_ABBREV_DEFAULT, error;
	git_oid id = {{0}};
	git_odb *odb;

	assert(out && obj);

	git_buf_sanitize(out);
	repo = git_object_owner(obj);

	if ((error = git_repository__cvar(&len, repo, GIT_CVAR_ABBREV)) < 0)
		return error;

	if ((error = git_repository_odb(&odb, repo)) < 0)
		return error;

	while (len < GIT_OID_HEXSZ) {
		/* set up short oid */
		memcpy(&id.id, &obj->cached.oid.id, (len + 1) / 2);
		if (len & 1)
			id.id[len / 2] &= 0xf0;

		error = git_odb_exists_prefix(NULL, odb, &id, len);
		if (error != GIT_EAMBIGUOUS)
			break;

		giterr_clear();
		len++;
	}

	if (!error && !(error = git_buf_grow(out, len + 1))) {
		git_oid_tostr(out->ptr, len + 1, &id);
		out->size = len;
	}

	git_odb_free(odb);

	return error;
}

static int object_parse_error(
	git_otype otype, git_object_parse_t *item, const char *msg)
{
	const char *typestr = git_object_type2string(otype);

	if (item->tag)
		giterr_set(GITERR_OBJECT, "Failed to parse %s - %s '%s'",
			typestr, msg, item->tag);
	else
		giterr_set(GITERR_OBJECT, "Failed to parse %s - %s", typestr, msg);

	return -1;
}

static int object_parse_line(
	git_otype otype,
	git_object_parse_t *item,
	const char *buf,
	const char *eol,
	int error)
{
	size_t len;
	const char *msg = NULL;

	buf += item->taglen + 1;

	if (eol <= buf) {
		msg = "insufficient data for";
		goto done;
	} else
		len = (size_t)(eol - buf);

	switch (item->type) {
	case GIT_PARSE_OID:
	case GIT_PARSE_OID_ARRAY: {
		git_oid *id = (item->type == GIT_PARSE_OID) ?
			item->value.id : git_array_alloc(*item->value.ids);

		if (!id)
			msg = "out of memory";
		else if (len < GIT_OID_HEXSZ)
			msg = "insufficient data for";
		else if (git_oid_fromstr(id, buf) < 0)
			msg = "invalid OID in";
		else if (len > GIT_OID_HEXSZ + 1)
			msg = "extra data after";
		else if (buf[GIT_OID_HEXSZ] != '\n')
			msg = "improper termination for";
		break;
	}
	case GIT_PARSE_OTYPE:
		if ((*item->value.otype = git_object_string2type(buf, len)) ==
			GIT_OBJ_BAD)
			msg = "invalid value for";
		break;
	case GIT_PARSE_SIGNATURE:
		*item->value.sig = git__calloc(1, sizeof(git_signature));
		if (!*item->value.sig)
			msg = "out of memory";
		else if (git_signature__parse(
				*item->value.sig, &buf, eol + 1, NULL, '\n') < 0)
			msg = "invalid signature for";
		break;
	case GIT_PARSE_TO_EOL:
		if (eol[-1] == '\r')
			--len;
		if ((*item->value.text = git__strndup(buf, len)) == NULL)
			msg = "out of memory";
		break;
	default:
		msg = "unexpected parse type";
		break;
	}

done:
	if (msg && !error)
		error = object_parse_error(otype, item, msg);
	return error;
}

int git_object__parse_lines(
	git_otype otype,
	git_object_parse_t *parse,
	const char *buf,
	const char *buf_end)
{
	int error = 0;
	bool optional = false;
	char *eol;
	git_object_parse_t *scan = parse, *next = parse + 1;
	size_t len;

	/* process required and optional lines */
	for (; buf < buf_end && scan->type > GIT_PARSE_BODY; scan = (next++)) {
		len = buf_end - buf;

		if (scan->type == GIT_PARSE_MODE_OPTIONAL) {
			optional = true;
			continue;
		}

		if (git__iseol(buf, buf_end - buf))
			goto body;

		if ((eol = memchr(buf, '\n', buf_end - buf)) == NULL) {
			if (!error)
				error = object_parse_error(otype, scan, "unterminated line");
			break;
		}
		len = (size_t)(eol - buf);

		if (len > scan->taglen &&
			!memcmp(scan->tag, buf, scan->taglen) &&
			buf[scan->taglen] == ' ')
		{
			error = object_parse_line(otype, scan, buf, eol, error);

			if (scan->type == GIT_PARSE_OID_ARRAY) /* don't advance yet */
				next = scan;
		}
		else if (optional)
			/* for now, skip this tag - eventually search tags? */
			next = scan;
		else if (scan->type == GIT_PARSE_OID_ARRAY)
			continue;
		else if (!error)
			error = object_parse_error(
				otype, scan, "missing required field");

		buf = eol + 1; /* advance to next line */
	}

body:

	if (scan->type > GIT_PARSE_BODY) {
		if (!optional && !error)
			error = object_parse_error
				(otype, scan, "missing required field");

		while (scan->type > GIT_PARSE_BODY)
			scan++;
	}

	if (scan->type > GIT_PARSE_BODY)
		return error;

	while (buf < buf_end && !git__iseol(buf, buf_end - buf)) {
		if ((eol = memchr(buf, '\n', buf_end - buf)) == NULL)
			buf = buf_end;
		else
			buf = eol + 1;
	}

	if (buf < buf_end)
		buf += (*buf == '\n') ? 1 : 2;
	else {
		buf = buf_end;

		if (!error && scan->type != GIT_PARSE_BODY_OPTIONAL)
			error = object_parse_error(otype, scan, "missing message body");
	}

	if (scan->value.body)
		*scan->value.body = buf;

	return error;
}
