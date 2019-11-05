/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_refs_h__
#define INCLUDE_refs_h__

#include "common.h"

#include "git2/oid.h"
#include "git2/refs.h"
#include "git2/refdb.h"
#include "strmap.h"
#include "buffer.h"
#include "layout.h"
#include "oid.h"

extern bool git_reference__enable_symbolic_ref_target_validation;

#define GIT_SYMREF "ref: "
#define GIT_PACKEDREFS_HEADER "# pack-refs with: peeled fully-peeled sorted "

#define GIT_REFERENCE_FORMAT__PRECOMPOSE_UNICODE	(1u << 16)
#define GIT_REFERENCE_FORMAT__VALIDATION_DISABLE	(1u << 15)

#define GIT_REFNAME_MAX 1024

typedef char git_refname_t[GIT_REFNAME_MAX];

struct git_reference {
	git_refdb *db;
	git_reference_t type;

	union {
		git_oid oid;
		char *symbolic;
	} target;

	git_oid peel;
	char name[GIT_FLEX_ARRAY];
};

git_reference *git_reference__set_name(git_reference *ref, const char *name);

int git_reference__normalize_name(git_buf *buf, const char *name, unsigned int flags);
int git_reference__update_terminal(git_repository *repo, const char *ref_name, const git_oid *oid, const git_signature *sig, const char *log_message);
int git_reference__is_valid_name(const char *refname, unsigned int flags);
int git_reference__is_branch(const char *ref_name);
int git_reference__is_remote(const char *ref_name);
int git_reference__is_tag(const char *ref_name);
const char *git_reference__shorthand(const char *name);

/**
 * Lookup a reference by name and try to resolve to an OID.
 *
 * You can control how many dereferences this will attempt to resolve the
 * reference with the `max_deref` parameter, or pass -1 to use a sane
 * default.  If you pass 0 for `max_deref`, this will not attempt to resolve
 * the reference.  For any value of `max_deref` other than 0, not
 * successfully resolving the reference will be reported as an error.

 * The generated reference must be freed by the user.
 *
 * @param reference_out Pointer to the looked-up reference
 * @param repo The repository to look up the reference
 * @param name The long name for the reference (e.g. HEAD, ref/heads/master, refs/tags/v0.1.0, ...)
 * @param max_deref Maximum number of dereferences to make of symbolic refs, 0 means simple lookup, < 0 means use default reasonable value
 * @return 0 on success or < 0 on error; not being able to resolve the reference is an error unless 0 was passed for max_deref
 */
int git_reference_lookup_resolved(
	git_reference **reference_out,
	git_repository *repo,
	const char *name,
	int max_deref);

/**
 * Read reference from a file.
 *
 * This function will read in the file at `path`. If it is a
 * symref, it will return a new unresolved symbolic reference
 * with the given name pointing to the reference pointed to by
 * the file. If it is not a symbolic reference, it will return
 * the resolved reference.
 *
 * Note that because the refdb is not involved for symbolic references, they
 * won't be owned, hence you should either not make the returned reference
 * 'externally visible', or perform the lookup before returning it to the user.
 */
int git_reference__read_head(
	git_reference **out,
	git_repository *repo,
	const char *path);

int git_reference__log_signature(git_signature **out, git_repository *repo);

/** Update a reference after a commit. */
int git_reference__update_for_commit(
	git_repository *repo,
	git_reference *ref,
	const char *ref_name,
	const git_oid *id,
	const char *operation);

int git_reference__is_unborn_head(bool *unborn, const git_reference *ref, git_repository *repo);

#endif
