/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "refs.h"
#include "hash.h"
#include "repository.h"
#include "fileops.h"
#include "pack.h"
#include "reflog.h"
#include "refdb.h"

#include <git2/tag.h>
#include <git2/object.h>
#include <git2/oid.h>
#include <git2/branch.h>
#include <git2/refs.h>
#include <git2/refdb.h>
#include <git2/refdb_backend.h>

GIT__USE_STRMAP;

#define DEFAULT_NESTING_LEVEL	5
#define MAX_NESTING_LEVEL		10

enum {
	GIT_PACKREF_HAS_PEEL = 1,
	GIT_PACKREF_WAS_LOOSE = 2
};


git_reference *git_reference__alloc(
	git_refdb *refdb,
	const char *name,
	const git_oid *oid,
	const char *symbolic)
{
	git_reference *ref;
	size_t namelen;

	assert(refdb && name && ((oid && !symbolic) || (!oid && symbolic)));

	namelen = strlen(name);

	if ((ref = git__calloc(1, sizeof(git_reference) + namelen + 1)) == NULL)
		return NULL;

	if (oid) {
		ref->type = GIT_REF_OID;
		git_oid_cpy(&ref->target.oid, oid);
	} else {
		ref->type = GIT_REF_SYMBOLIC;

		if ((ref->target.symbolic = git__strdup(symbolic)) == NULL) {
			git__free(ref);
			return NULL;
		}
	}

	ref->db = refdb;
	memcpy(ref->name, name, namelen + 1);

	return ref;
}

void git_reference_free(git_reference *reference)
{
	if (reference == NULL)
		return;

	if (reference->type == GIT_REF_SYMBOLIC) {
		git__free(reference->target.symbolic);
		reference->target.symbolic = NULL;
	}

	reference->db = NULL;
	reference->type = GIT_REF_INVALID;

	git__free(reference);
}

struct reference_available_t {
	const char *new_ref;
	const char *old_ref;
	int available;
};

static int _reference_available_cb(const char *ref, void *data)
{
	struct reference_available_t *d;

	assert(ref && data);
	d = (struct reference_available_t *)data;

	if (!d->old_ref || strcmp(d->old_ref, ref)) {
		size_t reflen = strlen(ref);
		size_t newlen = strlen(d->new_ref);
		size_t cmplen = reflen < newlen ? reflen : newlen;
		const char *lead = reflen < newlen ? d->new_ref : ref;

		if (!strncmp(d->new_ref, ref, cmplen) && lead[cmplen] == '/') {
			d->available = 0;
			return -1;
		}
	}

	return 0;
}

static int reference_path_available(
	git_repository *repo,
	const char *ref,
	const char* old_ref)
{
	int error;
	struct reference_available_t data;

	data.new_ref = ref;
	data.old_ref = old_ref;
	data.available = 1;

	error = git_reference_foreach(
		repo, GIT_REF_LISTALL, _reference_available_cb, (void *)&data);
	if (error < 0)
		return error;

	if (!data.available) {
		giterr_set(GITERR_REFERENCE,
			"The path to reference '%s' collides with an existing one", ref);
		return -1;
	}

	return 0;
}

/*
 * Check if a reference could be written to disk, based on:
 *
 *	- Whether a reference with the same name already exists,
 *	and we are allowing or disallowing overwrites
 *
 *	- Whether the name of the reference would collide with
 *	an existing path
 */
static int reference_can_write(
	git_repository *repo,
	const char *refname,
	const char *previous_name,
	int force)
{
	git_refdb *refdb;

	if (git_repository_refdb__weakptr(&refdb, repo) < 0)
		return -1;

	/* see if the reference shares a path with an existing reference;
	 * if a path is shared, we cannot create the reference, even when forcing */
	if (reference_path_available(repo, refname, previous_name) < 0)
		return -1;

	/* check if the reference actually exists, but only if we are not forcing
	 * the rename. If we are forcing, it's OK to overwrite */
	if (!force) {
		int exists;

		if (git_refdb_exists(&exists, refdb, refname) < 0)
			return -1;

		/* We cannot proceed if the reference already exists and we're not forcing
		 * the rename; the existing one would be overwritten */
		if (exists) {
			giterr_set(GITERR_REFERENCE,
				"A reference with that name (%s) already exists", refname);
			return GIT_EEXISTS;
		}
	}

	/* FIXME: if the reference exists and we are forcing, do we really need to
	 * remove the reference first?
	 *
	 * Two cases:
	 *
	 *	- the reference already exists and is loose: not a problem, the file
	 *	gets overwritten on disk
	 *
	 *	- the reference already exists and is packed: we write a new one as
	 *	loose, which by all means renders the packed one useless
	 */

	return 0;
}

int git_reference_delete(git_reference *ref)
{
	return git_refdb_delete(ref->db, ref);
}

int git_reference_lookup(git_reference **ref_out,
	git_repository *repo, const char *name)
{
	return git_reference_lookup_resolved(ref_out, repo, name, 0);
}

int git_reference_name_to_id(
	git_oid *out, git_repository *repo, const char *name)
{
	int error;
	git_reference *ref;

	if ((error = git_reference_lookup_resolved(&ref, repo, name, -1)) < 0)
		return error;

	git_oid_cpy(out, git_reference_target(ref));
	git_reference_free(ref);
	return 0;
}

int git_reference_lookup_resolved(
	git_reference **ref_out,
	git_repository *repo,
	const char *name,
	int max_nesting)
{
	char scan_name[GIT_REFNAME_MAX];
	git_ref_t scan_type;
	int error = 0, nesting;
	git_reference *ref = NULL;
	git_refdb *refdb;

	assert(ref_out && repo && name);

	*ref_out = NULL;

	if (max_nesting > MAX_NESTING_LEVEL)
		max_nesting = MAX_NESTING_LEVEL;
	else if (max_nesting < 0)
		max_nesting = DEFAULT_NESTING_LEVEL;
	
	strncpy(scan_name, name, GIT_REFNAME_MAX);
	scan_type = GIT_REF_SYMBOLIC;
	
	if ((error = git_repository_refdb__weakptr(&refdb, repo)) < 0)
		return -1;

	if ((error = git_reference__normalize_name_lax(scan_name, GIT_REFNAME_MAX, name)) < 0)
		return error;

	for (nesting = max_nesting;
		 nesting >= 0 && scan_type == GIT_REF_SYMBOLIC;
		 nesting--)
	{
		if (nesting != max_nesting) {
			strncpy(scan_name, ref->target.symbolic, GIT_REFNAME_MAX);
			git_reference_free(ref);
		}

		if ((error = git_refdb_lookup(&ref, refdb, scan_name)) < 0)
			return error;
		
		scan_type = ref->type;
	}

	if (scan_type != GIT_REF_OID && max_nesting != 0) {
		giterr_set(GITERR_REFERENCE,
			"Cannot resolve reference (>%u levels deep)", max_nesting);
		git_reference_free(ref);
		return -1;
	}

	*ref_out = ref;
	return 0;
}

/**
 * Getters
 */
git_ref_t git_reference_type(const git_reference *ref)
{
	assert(ref);
	return ref->type;
}

const char *git_reference_name(const git_reference *ref)
{
	assert(ref);
	return ref->name;
}

git_repository *git_reference_owner(const git_reference *ref)
{
	assert(ref);
	return ref->db->repo;
}

const git_oid *git_reference_target(const git_reference *ref)
{
	assert(ref);

	if (ref->type != GIT_REF_OID)
		return NULL;

	return &ref->target.oid;
}

const char *git_reference_symbolic_target(const git_reference *ref)
{
	assert(ref);

	if (ref->type != GIT_REF_SYMBOLIC)
		return NULL;

	return ref->target.symbolic;
}

static int reference__create(
	git_reference **ref_out,
	git_repository *repo,
	const char *name,
	const git_oid *oid,
	const char *symbolic,
	int force)
{
	char normalized[GIT_REFNAME_MAX];
	git_refdb *refdb;
	git_reference *ref = NULL;
	int error = 0;
	
	if (ref_out)
		*ref_out = NULL;

	if ((error = git_reference__normalize_name_lax(normalized, sizeof(normalized), name)) < 0 ||
		(error = reference_can_write(repo, normalized, NULL, force)) < 0 ||
		(error = git_repository_refdb__weakptr(&refdb, repo)) < 0)
		return error;
	
	if ((ref = git_reference__alloc(refdb, name, oid, symbolic)) == NULL)
		return -1;

	if ((error = git_refdb_write(refdb, ref)) < 0) {
		git_reference_free(ref);
		return error;
	}
	
	if (ref_out == NULL)
		git_reference_free(ref);
	else
		*ref_out = ref;

	return 0;
}

int git_reference_create(
	git_reference **ref_out,
	git_repository *repo,
	const char *name,
	const git_oid *oid,
	int force)
{
	git_odb *odb;
	int error = 0;

	assert(repo && name && oid);
	
	/* Sanity check the reference being created - target must exist. */
	if ((error = git_repository_odb__weakptr(&odb, repo)) < 0)
		return error;
	
	if (!git_odb_exists(odb, oid)) {
		giterr_set(GITERR_REFERENCE,
			"Target OID for the reference doesn't exist on the repository");
		return -1;
	}
	
	return reference__create(ref_out, repo, name, oid, NULL, force);
}

int git_reference_symbolic_create(
	git_reference **ref_out,
	git_repository *repo,
	const char *name,
	const char *target,
	int force)
{
	char normalized[GIT_REFNAME_MAX];
	int error = 0;

	assert(repo && name && target);
	
	if ((error = git_reference__normalize_name_lax(
		normalized, sizeof(normalized), target)) < 0)
		return error;

	return reference__create(ref_out, repo, name, NULL, normalized, force);
}

int git_reference_set_target(
	git_reference **out,
	git_reference *ref,
	const git_oid *id)
{
	assert(out && ref && id);
	
	if (ref->type != GIT_REF_OID) {
		giterr_set(GITERR_REFERENCE, "Cannot set OID on symbolic reference");
		return -1;
	}

	return git_reference_create(out, ref->db->repo, ref->name, id, 1);
}

int git_reference_symbolic_set_target(
	git_reference **out,
	git_reference *ref,
	const char *target)
{
	assert(out && ref && target);
	
	if (ref->type != GIT_REF_SYMBOLIC) {
		giterr_set(GITERR_REFERENCE,
			"Cannot set symbolic target on a direct reference");
		return -1;
	}
	
	return git_reference_symbolic_create(out, ref->db->repo, ref->name, target, 1);
}

int git_reference_rename(
	git_reference **out,
	git_reference *ref,
	const char *new_name,
	int force)
{
	unsigned int normalization_flags;
	char normalized[GIT_REFNAME_MAX];
	bool should_head_be_updated = false;
	git_reference *result = NULL;
	git_oid *oid;
	const char *symbolic;
	int error = 0;
	int reference_has_log;
	
	*out = NULL;

	normalization_flags = ref->type == GIT_REF_SYMBOLIC ?
		GIT_REF_FORMAT_ALLOW_ONELEVEL : GIT_REF_FORMAT_NORMAL;

	if ((error = git_reference_normalize_name(normalized, sizeof(normalized), new_name, normalization_flags)) < 0 ||
		(error = reference_can_write(ref->db->repo, normalized, ref->name, force)) < 0)
		return error;

	/*
	 * Create the new reference.
	 */
	if (ref->type == GIT_REF_OID) {
		oid = &ref->target.oid;
		symbolic = NULL;
	} else {
		oid = NULL;
		symbolic = ref->target.symbolic;
	}
	
	if ((result = git_reference__alloc(ref->db, new_name, oid, symbolic)) == NULL)
		return -1;

	/* Check if we have to update HEAD. */
	if ((error = git_branch_is_head(ref)) < 0)
		goto on_error;

	should_head_be_updated = (error > 0);

	/* Now delete the old ref and save the new one. */
	if ((error = git_refdb_delete(ref->db, ref)) < 0)
		goto on_error;
	
	/* Save the new reference. */
	if ((error = git_refdb_write(ref->db, result)) < 0)
		goto rollback;
	
	/* Update HEAD it was poiting to the reference being renamed. */
	if (should_head_be_updated && (error = git_repository_set_head(ref->db->repo, new_name)) < 0) {
		giterr_set(GITERR_REFERENCE, "Failed to update HEAD after renaming reference");
		goto on_error;
	}

	/* Rename the reflog file, if it exists. */
	reference_has_log = git_reference_has_log(ref);
	if (reference_has_log < 0) {
		error = reference_has_log;
		goto on_error;
	}
	if (reference_has_log && (error = git_reflog_rename(ref, new_name)) < 0)
		goto on_error;

	*out = result;

	return error;

rollback:
	git_refdb_write(ref->db, ref);

on_error:
	git_reference_free(result);

	return error;
}

int git_reference_resolve(git_reference **ref_out, const git_reference *ref)
{
	if (ref->type == GIT_REF_OID)
		return git_reference_lookup(ref_out, ref->db->repo, ref->name);
	else
		return git_reference_lookup_resolved(ref_out, ref->db->repo,
			ref->target.symbolic, -1);
}

int git_reference_foreach(
	git_repository *repo,
	unsigned int list_flags,
	git_reference_foreach_cb callback,
	void *payload)
{
	git_refdb *refdb;
	git_repository_refdb__weakptr(&refdb, repo);

	return git_refdb_foreach(refdb, list_flags, callback, payload);
}

static int cb__reflist_add(const char *ref, void *data)
{
	return git_vector_insert((git_vector *)data, git__strdup(ref));
}

int git_reference_list(
	git_strarray *array,
	git_repository *repo,
	unsigned int list_flags)
{
	git_vector ref_list;

	assert(array && repo);

	array->strings = NULL;
	array->count = 0;

	if (git_vector_init(&ref_list, 8, NULL) < 0)
		return -1;

	if (git_reference_foreach(
			repo, list_flags, &cb__reflist_add, (void *)&ref_list) < 0) {
		git_vector_free(&ref_list);
		return -1;
	}

	array->strings = (char **)ref_list.contents;
	array->count = ref_list.length;
	return 0;
}

static int is_valid_ref_char(char ch)
{
	if ((unsigned) ch <= ' ')
		return 0;

	switch (ch) {
	case '~':
	case '^':
	case ':':
	case '\\':
	case '?':
	case '[':
	case '*':
		return 0;
	default:
		return 1;
	}
}

static int ensure_segment_validity(const char *name)
{
	const char *current = name;
	char prev = '\0';
	const int lock_len = (int)strlen(GIT_FILELOCK_EXTENSION);
	int segment_len;

	if (*current == '.')
		return -1; /* Refname starts with "." */

	for (current = name; ; current++) {
		if (*current == '\0' || *current == '/')
			break;

		if (!is_valid_ref_char(*current))
			return -1; /* Illegal character in refname */

		if (prev == '.' && *current == '.')
			return -1; /* Refname contains ".." */

		if (prev == '@' && *current == '{')
			return -1; /* Refname contains "@{" */

		prev = *current;
	}

	segment_len = (int)(current - name);

	/* A refname component can not end with ".lock" */
	if (segment_len >= lock_len &&
		!memcmp(current - lock_len, GIT_FILELOCK_EXTENSION, lock_len))
			return -1;

	return segment_len;
}

static bool is_all_caps_and_underscore(const char *name, size_t len)
{
	size_t i;
	char c;

	assert(name && len > 0);

	for (i = 0; i < len; i++)
	{
		c = name[i];
		if ((c < 'A' || c > 'Z') && c != '_')
			return false;
	}

	if (*name == '_' || name[len - 1] == '_')
		return false;

	return true;
}

int git_reference__normalize_name(
	git_buf *buf,
	const char *name,
	unsigned int flags)
{
	// Inspired from https://github.com/git/git/blob/f06d47e7e0d9db709ee204ed13a8a7486149f494/refs.c#L36-100

	char *current;
	int segment_len, segments_count = 0, error = GIT_EINVALIDSPEC;
	unsigned int process_flags;
	bool normalize = (buf != NULL);
	assert(name);

	process_flags = flags;
	current = (char *)name;

	if (*current == '/')
		goto cleanup;

	if (normalize)
		git_buf_clear(buf);

	while (true) {
		segment_len = ensure_segment_validity(current);
		if (segment_len < 0) {
			if ((process_flags & GIT_REF_FORMAT_REFSPEC_PATTERN) &&
					current[0] == '*' &&
					(current[1] == '\0' || current[1] == '/')) {
				/* Accept one wildcard as a full refname component. */
				process_flags &= ~GIT_REF_FORMAT_REFSPEC_PATTERN;
				segment_len = 1;
			} else
				goto cleanup;
		}

		if (segment_len > 0) {
			if (normalize) {
				size_t cur_len = git_buf_len(buf);

				git_buf_joinpath(buf, git_buf_cstr(buf), current);
				git_buf_truncate(buf,
					cur_len + segment_len + (segments_count ? 1 : 0));

				if (git_buf_oom(buf)) {
					error = -1;
					goto cleanup;
				}
			}

			segments_count++;
		}

		/* No empty segment is allowed when not normalizing */
		if (segment_len == 0 && !normalize)
			goto cleanup;

		if (current[segment_len] == '\0')
			break;

		current += segment_len + 1;
	}

	/* A refname can not be empty */
	if (segment_len == 0 && segments_count == 0)
		goto cleanup;

	/* A refname can not end with "." */
	if (current[segment_len - 1] == '.')
		goto cleanup;

	/* A refname can not end with "/" */
	if (current[segment_len - 1] == '/')
		goto cleanup;

	if ((segments_count == 1 ) && !(flags & GIT_REF_FORMAT_ALLOW_ONELEVEL))
		goto cleanup;

	if ((segments_count == 1 ) &&
		!(is_all_caps_and_underscore(name, (size_t)segment_len) ||
			((flags & GIT_REF_FORMAT_REFSPEC_PATTERN) && !strcmp("*", name))))
			goto cleanup;

	if ((segments_count > 1)
		&& (is_all_caps_and_underscore(name, strchr(name, '/') - name)))
			goto cleanup;

	error = 0;

cleanup:
	if (error == GIT_EINVALIDSPEC)
		giterr_set(
			GITERR_REFERENCE,
			"The given reference name '%s' is not valid", name);

	if (error && normalize)
		git_buf_free(buf);

	return error;
}

int git_reference_normalize_name(
	char *buffer_out,
	size_t buffer_size,
	const char *name,
	unsigned int flags)
{
	git_buf buf = GIT_BUF_INIT;
	int error;

	if ((error = git_reference__normalize_name(&buf, name, flags)) < 0)
		goto cleanup;

	if (git_buf_len(&buf) > buffer_size - 1) {
		giterr_set(
		GITERR_REFERENCE,
		"The provided buffer is too short to hold the normalization of '%s'", name);
		error = GIT_EBUFS;
		goto cleanup;
	}

	git_buf_copy_cstr(buffer_out, buffer_size, &buf);

	error = 0;

cleanup:
	git_buf_free(&buf);
	return error;
}

int git_reference__normalize_name_lax(
	char *buffer_out,
	size_t out_size,
	const char *name)
{
	return git_reference_normalize_name(
		buffer_out,
		out_size,
		name,
		GIT_REF_FORMAT_ALLOW_ONELEVEL);
}
#define GIT_REF_TYPEMASK (GIT_REF_OID | GIT_REF_SYMBOLIC)

int git_reference_cmp(git_reference *ref1, git_reference *ref2)
{
	assert(ref1 && ref2);

	/* let's put symbolic refs before OIDs */
	if (ref1->type != ref2->type)
		return (ref1->type == GIT_REF_SYMBOLIC) ? -1 : 1;

	if (ref1->type == GIT_REF_SYMBOLIC)
		return strcmp(ref1->target.symbolic, ref2->target.symbolic);

	return git_oid_cmp(&ref1->target.oid, &ref2->target.oid);
}

static int reference__update_terminal(
	git_repository *repo,
	const char *ref_name,
	const git_oid *oid,
	int nesting)
{
	git_reference *ref;
	int error = 0;

	if (nesting > MAX_NESTING_LEVEL)
		return GIT_ENOTFOUND;
	
	error = git_reference_lookup(&ref, repo, ref_name);

	/* If we haven't found the reference at all, create a new reference. */
	if (error == GIT_ENOTFOUND) {
		giterr_clear();
		return git_reference_create(NULL, repo, ref_name, oid, 0);
	}
	
	if (error < 0)
		return error;
	
	/* If the ref is a symbolic reference, follow its target. */
	if (git_reference_type(ref) == GIT_REF_SYMBOLIC) {
		error = reference__update_terminal(repo, git_reference_symbolic_target(ref), oid,
			nesting+1);
		git_reference_free(ref);
	} else {
		git_reference_free(ref);
		error = git_reference_create(NULL, repo, ref_name, oid, 1);
	}
	
	return error;
}

/*
 * Starting with the reference given by `ref_name`, follows symbolic
 * references until a direct reference is found and updated the OID
 * on that direct reference to `oid`.
 */
int git_reference__update_terminal(
	git_repository *repo,
	const char *ref_name,
	const git_oid *oid)
{
	return reference__update_terminal(repo, ref_name, oid, 0);
}

int git_reference_foreach_glob(
	git_repository *repo,
	const char *glob,
	unsigned int list_flags,
	int (*callback)(
		const char *reference_name,
		void *payload),
	void *payload)
{
	git_refdb *refdb;

	assert(repo && glob && callback);

	git_repository_refdb__weakptr(&refdb, repo);

	return git_refdb_foreach_glob(refdb, glob, list_flags, callback, payload);
}

int git_reference_has_log(
	git_reference *ref)
{
	git_buf path = GIT_BUF_INIT;
	int result;

	assert(ref);

	if (git_buf_join_n(&path, '/', 3, ref->db->repo->path_repository,
		GIT_REFLOG_DIR, ref->name) < 0)
		return -1;

	result = git_path_isfile(git_buf_cstr(&path));
	git_buf_free(&path);

	return result;
}

int git_reference__is_branch(const char *ref_name)
{
	return git__prefixcmp(ref_name, GIT_REFS_HEADS_DIR) == 0;
}

int git_reference_is_branch(git_reference *ref)
{
	assert(ref);
	return git_reference__is_branch(ref->name);
}

int git_reference__is_remote(const char *ref_name)
{
	return git__prefixcmp(ref_name, GIT_REFS_REMOTES_DIR) == 0;
}

int git_reference_is_remote(git_reference *ref)
{
	assert(ref);
	return git_reference__is_remote(ref->name);
}

static int peel_error(int error, git_reference *ref, const char* msg)
{
	giterr_set(
		GITERR_INVALID,
		"The reference '%s' cannot be peeled - %s", git_reference_name(ref), msg);
	return error;
}

static int reference_target(git_object **object, git_reference *ref)
{
	const git_oid *oid;

	oid = git_reference_target(ref);

	return git_object_lookup(object, git_reference_owner(ref), oid, GIT_OBJ_ANY);
}

int git_reference_peel(
		git_object **peeled,
		git_reference *ref,
		git_otype target_type)
{
	git_reference *resolved = NULL;
	git_object *target = NULL;
	int error;

	assert(ref);

	if ((error = git_reference_resolve(&resolved, ref)) < 0)
		return peel_error(error, ref, "Cannot resolve reference");

	if ((error = reference_target(&target, resolved)) < 0) {
		peel_error(error, ref, "Cannot retrieve reference target");
		goto cleanup;
	}

	if (target_type == GIT_OBJ_ANY && git_object_type(target) != GIT_OBJ_TAG)
		error = git_object__dup(peeled, target);
	else
		error = git_object_peel(peeled, target, target_type);

cleanup:
	git_object_free(target);
	git_reference_free(resolved);
	return error;
}

int git_reference__is_valid_name(
	const char *refname,
	unsigned int flags)
{
	int error;

	error = git_reference__normalize_name(NULL, refname, flags) == 0;
	giterr_clear();

	return error;
}

int git_reference_is_valid_name(
	const char *refname)
{
	return git_reference__is_valid_name(
		refname,
		GIT_REF_FORMAT_ALLOW_ONELEVEL);
}
