/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "object.h"

#include "git2/object.h"

#include "repository.h"

#include "buf.h"
#include "commit.h"
#include "hash.h"
#include "tree.h"
#include "blob.h"
#include "oid.h"
#include "tag.h"

bool git_object__strict_input_validation = true;

size_t git_object__size(git_object_t type);

typedef struct {
	const char	*str;	/* type name string */
	size_t		size;	/* size in bytes of the object structure */

	int  (*parse)(void *self, git_odb_object *obj, git_oid_t oid_type);
	int  (*parse_raw)(void *self, const char *data, size_t size, git_oid_t oid_type);
	void (*free)(void *self);
} git_object_def;

static git_object_def git_objects_table[] = {
	/* 0 = unused */
	{ "", 0, NULL, NULL, NULL },

	/* 1 = GIT_OBJECT_COMMIT */
	{ "commit", sizeof(git_commit), git_commit__parse, git_commit__parse_raw, git_commit__free },

	/* 2 = GIT_OBJECT_TREE */
	{ "tree", sizeof(git_tree), git_tree__parse, git_tree__parse_raw, git_tree__free },

	/* 3 = GIT_OBJECT_BLOB */
	{ "blob", sizeof(git_blob), git_blob__parse, git_blob__parse_raw, git_blob__free },

	/* 4 = GIT_OBJECT_TAG */
	{ "tag", sizeof(git_tag), git_tag__parse, git_tag__parse_raw, git_tag__free }
};

int git_object__from_raw(
	git_object **object_out,
	const char *data,
	size_t size,
	git_object_t object_type,
	git_oid_t oid_type)
{
	git_object_id_options id_opts = GIT_OBJECT_ID_OPTIONS_INIT;
	git_object_def *def;
	git_object *object;
	size_t object_size;
	int error;

	GIT_ASSERT_ARG(object_out);
	*object_out = NULL;

	id_opts.object_type = object_type;
	id_opts.oid_type = oid_type;

	/* Validate type match */
	if (object_type != GIT_OBJECT_BLOB &&
	    object_type != GIT_OBJECT_TREE &&
	    object_type != GIT_OBJECT_COMMIT &&
	    object_type != GIT_OBJECT_TAG) {
		git_error_set(GIT_ERROR_INVALID, "the requested type is invalid");
		return GIT_ENOTFOUND;
	}

	if ((object_size = git_object__size(object_type)) == 0) {
		git_error_set(GIT_ERROR_INVALID, "the requested type is invalid");
		return GIT_ENOTFOUND;
	}

	/* Allocate and initialize base object */
	object = git__calloc(1, object_size);
	GIT_ERROR_CHECK_ALLOC(object);
	object->cached.flags = GIT_CACHE_STORE_PARSED;
	object->cached.type = object_type;

	if ((error = git_object_id_from_buffer(&object->cached.oid,
			data, size, &id_opts)) < 0)
		return error;

	/* Parse raw object data */
	def = &git_objects_table[object_type];
	GIT_ASSERT(def->free && def->parse_raw);

	if ((error = def->parse_raw(object, data, size, oid_type)) < 0) {
		def->free(object);
		return error;
	}

	git_cached_obj_incref(object);
	*object_out = object;

	return 0;
}

int git_object__init_from_odb_object(
	git_object **object_out,
	git_repository *repo,
	git_odb_object *odb_obj,
	git_object_t type)
{
	size_t object_size;
	git_object *object = NULL;

	GIT_ASSERT_ARG(object_out);
	*object_out = NULL;

	/* Validate type match */
	if (type != GIT_OBJECT_ANY && type != odb_obj->cached.type) {
		git_error_set(GIT_ERROR_INVALID,
			"the requested type does not match the type in the ODB");
		return GIT_ENOTFOUND;
	}

	if ((object_size = git_object__size(odb_obj->cached.type)) == 0) {
		git_error_set(GIT_ERROR_INVALID, "the requested type is invalid");
		return GIT_ENOTFOUND;
	}

	/* Allocate and initialize base object */
	object = git__calloc(1, object_size);
	GIT_ERROR_CHECK_ALLOC(object);

	git_oid_cpy(&object->cached.oid, &odb_obj->cached.oid);
	object->cached.type = odb_obj->cached.type;
	object->cached.size = odb_obj->cached.size;
	object->repo = repo;

	*object_out = object;
	return 0;
}

int git_object__from_odb_object(
	git_object **object_out,
	git_repository *repo,
	git_odb_object *odb_obj,
	git_object_t type)
{
	int error;
	git_object_def *def;
	git_object *object = NULL;

	if ((error = git_object__init_from_odb_object(&object, repo, odb_obj, type)) < 0)
		return error;

	/* Parse raw object data */
	def = &git_objects_table[odb_obj->cached.type];
	GIT_ASSERT(def->free && def->parse);

	if ((error = def->parse(object, odb_obj, repo->oid_type)) < 0) {
		/*
		 * parse returns EINVALID on invalid data; downgrade
		 * that to a normal -1 error code.
		 */
		def->free(object);
		return -1;
	}

	*object_out = git_cache_store_parsed(&repo->objects, object);
	return 0;
}

void git_object__free(void *obj)
{
	git_object_t type = ((git_object *)obj)->cached.type;

	if (type < 0 || ((size_t)type) >= ARRAY_SIZE(git_objects_table) ||
		!git_objects_table[type].free)
		git__free(obj);
	else
		git_objects_table[type].free(obj);
}

int git_object_lookup_prefix(
	git_object **object_out,
	git_repository *repo,
	const git_oid *id,
	size_t len,
	git_object_t type)
{
	git_object *object = NULL;
	git_odb *odb = NULL;
	git_odb_object *odb_obj = NULL;
	size_t oid_hexsize;
	int error = 0;

	GIT_ASSERT_ARG(repo);
	GIT_ASSERT_ARG(object_out);
	GIT_ASSERT_ARG(id);

	if (len < GIT_OID_MINPREFIXLEN) {
		git_error_set(GIT_ERROR_OBJECT, "ambiguous lookup - OID prefix is too short");
		return GIT_EAMBIGUOUS;
	}

	error = git_repository_odb__weakptr(&odb, repo);
	if (error < 0)
		return error;

	oid_hexsize = git_oid_hexsize(repo->oid_type);

	if (len > oid_hexsize)
		len = oid_hexsize;

	if (len == oid_hexsize) {
		git_cached_obj *cached = NULL;

		/* We want to match the full id : we can first look up in the cache,
		 * since there is no need to check for non ambiguousity
		 */
		cached = git_cache_get_any(&repo->objects, id);
		if (cached != NULL) {
			if (cached->flags == GIT_CACHE_STORE_PARSED) {
				object = (git_object *)cached;

				if (type != GIT_OBJECT_ANY && type != object->cached.type) {
					git_object_free(object);
					git_error_set(GIT_ERROR_INVALID,
						"the requested type does not match the type in the ODB");
					return GIT_ENOTFOUND;
				}

				*object_out = object;
				return 0;
			} else if (cached->flags == GIT_CACHE_STORE_RAW) {
				odb_obj = (git_odb_object *)cached;
			} else {
				GIT_ASSERT(!"Wrong caching type in the global object cache");
			}
		} else {
			/* Object was not found in the cache, let's explore the backends.
			 * We could just use git_odb_read_unique_short_oid,
			 * it is the same cost for packed and loose object backends,
			 * but it may be much more costly for sqlite and hiredis.
			 */
			error = git_odb_read(&odb_obj, odb, id);
		}
	} else {
		git_oid short_oid;

		git_oid_clear(&short_oid, repo->oid_type);
		git_oid__cpy_prefix(&short_oid, id, len);

		/* If len < GIT_OID_SHA1_HEXSIZE (a strict short oid was given), we have
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

	GIT_ASSERT(odb_obj);
	error = git_object__from_odb_object(object_out, repo, odb_obj, type);

	git_odb_object_free(odb_obj);

	return error;
}

int git_object_lookup(git_object **object_out, git_repository *repo, const git_oid *id, git_object_t type) {
	return git_object_lookup_prefix(object_out,
		repo, id, git_oid_hexsize(repo->oid_type), type);
}

void git_object_free(git_object *object)
{
	if (object == NULL)
		return;

	git_cached_obj_decref(object);
}

const git_oid *git_object_id(const git_object *obj)
{
	GIT_ASSERT_ARG_WITH_RETVAL(obj, NULL);
	return &obj->cached.oid;
}

git_object_t git_object_type(const git_object *obj)
{
	GIT_ASSERT_ARG_WITH_RETVAL(obj, GIT_OBJECT_INVALID);
	return obj->cached.type;
}

git_repository *git_object_owner(const git_object *obj)
{
	GIT_ASSERT_ARG_WITH_RETVAL(obj, NULL);
	return obj->repo;
}

const char *git_object_type2string(git_object_t type)
{
	if (type < 0 || ((size_t) type) >= ARRAY_SIZE(git_objects_table))
		return "";

	return git_objects_table[type].str;
}

git_object_t git_object_string2type(const char *str)
{
	if (!str)
		return GIT_OBJECT_INVALID;

	return git_object_stringn2type(str, strlen(str));
}

git_object_t git_object_stringn2type(const char *str, size_t len)
{
	size_t i;

	if (!str || !len || !*str)
		return GIT_OBJECT_INVALID;

	for (i = 0; i < ARRAY_SIZE(git_objects_table); i++)
		if (*git_objects_table[i].str &&
			!git__prefixncmp(str, len, git_objects_table[i].str))
			return (git_object_t)i;

	return GIT_OBJECT_INVALID;
}

int git_object_type_is_valid(git_object_t type)
{
	if (type < 0 || ((size_t) type) >= ARRAY_SIZE(git_objects_table))
		return 0;

	return (git_objects_table[type].size > 0) ? 1 : 0;
}

#ifndef GIT_DEPRECATE_HARD
int git_object_typeisloose(git_object_t type)
{
	return git_object_type_is_valid(type);
}
#endif

size_t git_object__size(git_object_t type)
{
	if (type < 0 || ((size_t) type) >= ARRAY_SIZE(git_objects_table))
		return 0;

	return git_objects_table[type].size;
}

static int dereference_object(git_object **dereferenced, git_object *obj)
{
	git_object_t type = git_object_type(obj);

	switch (type) {
	case GIT_OBJECT_COMMIT:
		return git_commit_tree((git_tree **)dereferenced, (git_commit*)obj);

	case GIT_OBJECT_TAG:
		return git_tag_target(dereferenced, (git_tag*)obj);

	case GIT_OBJECT_BLOB:
	case GIT_OBJECT_TREE:
		return GIT_EPEEL;

	default:
		return GIT_EINVALIDSPEC;
	}
}

static int peel_error(int error, const git_oid *oid, git_object_t type)
{
	const char *type_name;
	char hex_oid[GIT_OID_MAX_HEXSIZE + 1];

	type_name = git_object_type2string(type);

	git_oid_nfmt(hex_oid, GIT_OID_MAX_HEXSIZE + 1, oid);

	git_error_set(GIT_ERROR_OBJECT, "the git_object of id '%s' can not be "
		"successfully peeled into a %s (git_object_t=%i).", hex_oid, type_name, type);

	return error;
}

static int check_type_combination(git_object_t type, git_object_t target)
{
	if (type == target)
		return 0;

	switch (type) {
	case GIT_OBJECT_BLOB:
	case GIT_OBJECT_TREE:
		/* a blob or tree can never be peeled to anything but themselves */
		return GIT_EINVALIDSPEC;
		break;
	case GIT_OBJECT_COMMIT:
		/* a commit can only be peeled to a tree */
		if (target != GIT_OBJECT_TREE && target != GIT_OBJECT_ANY)
			return GIT_EINVALIDSPEC;
		break;
	case GIT_OBJECT_TAG:
		/* a tag may point to anything, so we let anything through */
		break;
	default:
		return GIT_EINVALIDSPEC;
	}

	return 0;
}

int git_object_peel(
	git_object **peeled,
	const git_object *object,
	git_object_t target_type)
{
	git_object *source, *deref = NULL;
	int error;

	GIT_ASSERT_ARG(object);
	GIT_ASSERT_ARG(peeled);

	GIT_ASSERT_ARG(target_type == GIT_OBJECT_TAG ||
		target_type == GIT_OBJECT_COMMIT ||
		target_type == GIT_OBJECT_TREE ||
		target_type == GIT_OBJECT_BLOB ||
		target_type == GIT_OBJECT_ANY);

	if ((error = check_type_combination(git_object_type(object), target_type)) < 0)
		return peel_error(error, git_object_id(object), target_type);

	if (git_object_type(object) == target_type)
		return git_object_dup(peeled, (git_object *)object);

	source = (git_object *)object;

	while (!(error = dereference_object(&deref, source))) {

		if (source != object)
			git_object_free(source);

		if (git_object_type(deref) == target_type) {
			*peeled = deref;
			return 0;
		}

		if (target_type == GIT_OBJECT_ANY &&
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
		git_object_t type)
{
	int error = -1;
	git_tree *tree = NULL;
	git_tree_entry *entry = NULL;

	GIT_ASSERT_ARG(out);
	GIT_ASSERT_ARG(treeish);
	GIT_ASSERT_ARG(path);

	if ((error = git_object_peel((git_object**)&tree, treeish, GIT_OBJECT_TREE)) < 0 ||
		 (error = git_tree_entry_bypath(&entry, tree, path)) < 0)
	{
		goto cleanup;
	}

	if (type != GIT_OBJECT_ANY && git_tree_entry_type(entry) != type)
	{
		git_error_set(GIT_ERROR_OBJECT,
				"object at path '%s' is not of the asked-for type %d",
				path, type);
		error = GIT_EINVALIDSPEC;
		goto cleanup;
	}

	error = git_tree_entry_to_object(out, git_object_owner(treeish), entry);

cleanup:
	git_tree_entry_free(entry);
	git_tree_free(tree);
	return error;
}

static int git_object__short_id(git_str *out, const git_object *obj)
{
	git_repository *repo;
	git_oid id;
	git_odb *odb;
	size_t oid_hexsize;
	int len, error;

	GIT_ASSERT_ARG(out);
	GIT_ASSERT_ARG(obj);

	repo = git_object_owner(obj);

	git_oid_clear(&id, repo->oid_type);
	oid_hexsize = git_oid_hexsize(repo->oid_type);

	if ((error = git_repository__abbrev_length(&len, repo)) < 0)
		return error;

	if ((size_t)len == oid_hexsize) {
		if ((error = git_oid_cpy(&id, &obj->cached.oid)) < 0) {
			return error;
		}
	}

	if ((error = git_repository_odb(&odb, repo)) < 0)
		return error;

	while ((size_t)len < oid_hexsize) {
		/* set up short oid */
		memcpy(&id.id, &obj->cached.oid.id, (len + 1) / 2);
		if (len & 1)
			id.id[len / 2] &= 0xf0;

		error = git_odb_exists_prefix(NULL, odb, &id, len);
		if (error != GIT_EAMBIGUOUS)
			break;

		git_error_clear();
		len++;
	}

	if (!error && !(error = git_str_grow(out, len + 1))) {
		git_oid_tostr(out->ptr, len + 1, &id);
		out->size = len;
	}

	git_odb_free(odb);

	return error;
}

int git_object_short_id(git_buf *out, const git_object *obj)
{
	GIT_BUF_WRAP_PRIVATE(out, git_object__short_id, obj);
}

bool git_object__is_valid(
	git_repository *repo, const git_oid *id, git_object_t expected_type)
{
	git_odb *odb;
	git_object_t actual_type;
	size_t len;
	int error;

	if (!git_object__strict_input_validation)
		return true;

	if ((error = git_repository_odb__weakptr(&odb, repo)) < 0 ||
		(error = git_odb_read_header(&len, &actual_type, odb, id)) < 0)
		return false;

	if (expected_type != GIT_OBJECT_ANY && expected_type != actual_type) {
		git_error_set(GIT_ERROR_INVALID,
			"the requested type does not match the type in the ODB");
		return false;
	}

	return true;
}

int git_object_id_options_init(
	git_object_id_options *opts,
	unsigned int version)
{
	GIT_INIT_STRUCTURE_FROM_TEMPLATE(opts, version,
		git_object_id_options, GIT_OBJECT_ID_OPTIONS_INIT);
	return 0;
}

GIT_INLINE(bool) needs_filter(git_object_id_options *opts)
{
	return (opts && opts->filters &&
	        git_filter_list_length(opts->filters) > 0 &&
	        (!opts->object_type || opts->object_type == GIT_OBJECT_BLOB));
}

GIT_INLINE(int) normalize_options(
	git_object_id_options *normalized,
	const git_object_id_options *given_opts)
{
	normalized->object_type = (given_opts && given_opts->object_type) ?
		given_opts->object_type : GIT_OBJECT_BLOB;

	normalized->oid_type = (given_opts && given_opts->oid_type) ?
		given_opts->oid_type : GIT_OID_DEFAULT;

	if (!git_object_type_is_valid(normalized->object_type)) {
		git_error_set(GIT_ERROR_INVALID, "invalid object type");
		return -1;
	}

	if (!git_oid_type_is_valid(normalized->oid_type)) {
		git_error_set(GIT_ERROR_INVALID, "unknown oid type");
		return -1;
	}

	if (given_opts)
		normalized->filters = given_opts->filters;

	return 0;
}

/* Raw object ID computation (no filters applied) from a descriptor */
static int id_from_fd(
	git_oid *out,
	git_file fd,
	size_t size,
	git_object_id_options *opts)
{
	size_t hdr_len;
	char hdr[64], buffer[GIT_BUFSIZE_FILEIO];
	git_hash_ctx ctx;
	git_hash_algorithm_t algorithm;
	ssize_t read_len = 0;
	int error = 0;

	algorithm = git_oid_algorithm(opts->oid_type);

	if ((error = git_hash_ctx_init(&ctx, algorithm)) < 0)
		return error;

	if ((error = git_odb__format_object_header(&hdr_len, hdr,
		sizeof(hdr), size, opts->object_type)) < 0)
		goto done;

	if ((error = git_hash_update(&ctx, hdr, hdr_len)) < 0)
		goto done;

	while (size > 0 &&
	       (read_len = p_read(fd, buffer, sizeof(buffer))) > 0) {

		if ((size_t)read_len > size) {
			git_error_set(GIT_ERROR_OS, "error reading file for hashing");
			error = -1;
			goto done;
		}

		if ((error = git_hash_update(&ctx, buffer, read_len)) < 0)
			goto done;

		size -= read_len;
	}

	/* If p_read returned an error code, the read obviously failed.
	 * If size is not zero, the file was truncated after we originally
	 * stat'd it, so we consider this a read failure too */
	if (read_len < 0 || size > 0) {
		git_error_set(GIT_ERROR_OS, "error reading file for hashing");
		error = -1;

		goto done;
	}

	error = git_hash_final(out->id, &ctx);

#ifdef GIT_EXPERIMENTAL_SHA256
	out->type = opts->oid_type;
#endif

done:
	git_hash_ctx_cleanup(&ctx);
	return error;
}

int git_object_id_from_fd(
	git_oid *id,
	git_file fd,
	size_t size,
	git_object_id_options *given_opts)
{
	git_object_id_options opts = GIT_OBJECT_ID_OPTIONS_INIT;
	git_str raw = GIT_STR_INIT;
	int error;

	GIT_ASSERT_ARG(id);

	if (normalize_options(&opts, given_opts) < 0)
		return -1;

	if (!needs_filter(&opts))
		return id_from_fd(id, fd, size, &opts);

	/*
	 * size of data is used in header, so we have to read the
	 * whole file into memory to apply filters before beginning
	 * to calculate the hash
	 */

	if ((error = git_futils_readbuffer_fd(&raw, fd, size)) < 0)
		goto done;

	error = git_object_id_from_buffer(id, raw.ptr, raw.size, &opts);

done:
	git_str_dispose(&raw);
	return error;
}

int git_object_id_from_symlink(
	git_oid *id,
	const char *path,
	const git_object_id_options *given_opts)
{
	git_object_id_options opts = GIT_OBJECT_ID_OPTIONS_INIT;
	struct stat st;
	char *link_data;
	size_t link_len;
	int read_len;
	int error;

	GIT_ASSERT_ARG(id);
	GIT_ASSERT_ARG(path);

	if (normalize_options(&opts, given_opts) < 0)
		return -1;

	opts.filters = NULL;
	if (opts.object_type != GIT_OBJECT_BLOB) {
		git_error_set(GIT_ERROR_INVALID, "symbolic links must be blob types");
		return -1;
	}

	if (git_fs_path_lstat(path, &st) < 0)
		return -1;

	/* Non-symlink fallback, primarily for non-Unix systems. */
	if (!S_ISLNK(st.st_mode))
		return git_object_id_from_file(id, path, &opts);

	if (!git__is_int(st.st_size) || (int)st.st_size < 0) {
		git_error_set(GIT_ERROR_FILESYSTEM, "file size overflow for 32-bit systems");
		return -1;
	}

	link_len = (size_t)st.st_size;
	link_data = git__malloc(link_len + 1);
	GIT_ERROR_CHECK_ALLOC(link_data);

	if ((read_len = p_readlink(path, link_data, link_len)) < 0) {
		git_error_set(GIT_ERROR_OS, "failed to read symlink data for '%s'", path);
		error = -1;
		goto done;
	}

	GIT_ASSERT(read_len <= st.st_size);
	link_data[read_len] = '\0';

	error = git_object_id_from_buffer(id,
		link_data, (size_t)read_len, &opts);

done:
	git__free(link_data);
	return error;
}

int git_object_id_from_file(
	git_oid *id,
	const char *path,
	const git_object_id_options *given_opts)
{
	git_object_id_options opts = GIT_OBJECT_ID_OPTIONS_INIT;
	uint64_t size;
	int fd, error = 0;

	GIT_ASSERT_ARG(id);
	GIT_ASSERT_ARG(path);

	if (normalize_options(&opts, given_opts) < 0)
		return -1;

	if ((fd = git_futils_open_ro(path)) < 0)
		return fd;

	if ((error = git_futils_filesize(&size, fd)) < 0)
		goto done;

	if (!git__is_sizet(size)) {
		git_error_set(GIT_ERROR_OS, "file size overflow for 32-bit systems");
		error = -1;
		goto done;
	}

	error = git_object_id_from_fd(id, fd, (size_t)size, &opts);

done:
	p_close(fd);
	return error;
}

/* Raw object ID computation (no filters applied) from a buffer */
static int id_from_buffer(
	git_oid *id,
	const void *data,
	size_t len,
	const git_object_id_options *opts)
{
	git_str_vec vec[2];
	char header[64];
	size_t hdrlen;
	git_hash_algorithm_t algorithm;
	int error;

	algorithm = git_oid_algorithm(opts->oid_type);

	if (!data && len != 0) {
		git_error_set(GIT_ERROR_INVALID, "invalid object");
		return -1;
	}

	if ((error = git_odb__format_object_header(&hdrlen,
		header, sizeof(header), len, opts->object_type)) < 0)
		return error;

	vec[0].data = header;
	vec[0].len = hdrlen;
	vec[1].data = (void *)data;
	vec[1].len = len;

#ifdef GIT_EXPERIMENTAL_SHA256
	id->type = opts->oid_type;
#endif

	return git_hash_vec(id->id, vec, 2, algorithm);
}

int git_object_id_from_buffer(
	git_oid *id,
	const void *data,
	size_t len,
	const git_object_id_options *given_opts)
{
	git_str filtered = GIT_STR_INIT;
	git_object_id_options opts = GIT_OBJECT_ID_OPTIONS_INIT;
	int error = -1;

	GIT_ASSERT_ARG(id);

	if (normalize_options(&opts, given_opts) < 0)
		return -1;

	if (needs_filter(&opts)) {
		if (git_filter_list__apply_to_buffer(&filtered,
				opts.filters, (char *)data, len) < 0)
			goto done;

		data = filtered.ptr;
		len = filtered.size;
	}

	error = id_from_buffer(id, data, len, &opts);

done:
	git_str_dispose(&filtered);
	return error;
}

int git_object_rawcontent_is_valid(
	int *valid,
	const char *buf,
	size_t len,
	git_object_t object_type
#ifdef GIT_EXPERIMENTAL_SHA256
	, git_oid_t oid_type
#endif
	)
{
	git_object *obj = NULL;
	int error;

#ifndef GIT_EXPERIMENTAL_SHA256
	git_oid_t oid_type = GIT_OID_SHA1;
#endif

	GIT_ASSERT_ARG(valid);
	GIT_ASSERT_ARG(buf);

	/* Blobs are always valid; don't bother parsing. */
	if (object_type == GIT_OBJECT_BLOB) {
		*valid = 1;
		return 0;
	}

	error = git_object__from_raw(&obj, buf, len, object_type, oid_type);
	git_object_free(obj);

	if (error == 0) {
		*valid = 1;
		return 0;
	} else if (error == GIT_EINVALID) {
		*valid = 0;
		return 0;
	}

	return error;
}

int git_object__parse_oid_header(
	git_oid *oid,
	const char **buffer_out,
	const char *buffer_end,
	const char *header,
	git_oid_t oid_type)
{
	const size_t sha_len = git_oid_hexsize(oid_type);
	const size_t header_len = strlen(header);

	const char *buffer = *buffer_out;

	if (buffer + (header_len + sha_len + 1) > buffer_end)
		return -1;

	if (memcmp(buffer, header, header_len) != 0)
		return -1;

	if (buffer[header_len + sha_len] != '\n')
		return -1;

	if (git_oid_from_prefix(oid, buffer + header_len, sha_len, oid_type) < 0)
		return -1;

	*buffer_out = buffer + (header_len + sha_len + 1);

	return 0;
}

int git_object__write_oid_header(
	git_str *buf,
	const char *header,
	const git_oid *oid)
{
	size_t hex_size = git_oid_hexsize(git_oid_type(oid));
	char hex_oid[GIT_OID_MAX_HEXSIZE];

	if (!hex_size) {
		git_error_set(GIT_ERROR_INVALID, "unknown type");
		return -1;
	}

	git_oid_fmt(hex_oid, oid);
	git_str_puts(buf, header);
	git_str_put(buf, hex_oid, hex_size);
	git_str_putc(buf, '\n');

	return git_str_oom(buf) ? -1 : 0;
}
