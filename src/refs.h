/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_refs_h__
#define INCLUDE_refs_h__

#include "common.h"
#include "git2/oid.h"
#include "git2/refs.h"
#include "strmap.h"

#define GIT_REFS_DIR "refs/"
#define GIT_REFS_HEADS_DIR GIT_REFS_DIR "heads/"
#define GIT_REFS_TAGS_DIR GIT_REFS_DIR "tags/"
#define GIT_REFS_REMOTES_DIR GIT_REFS_DIR "remotes/"
#define GIT_REFS_DIR_MODE 0777
#define GIT_REFS_FILE_MODE 0666

#define GIT_RENAMED_REF_FILE GIT_REFS_DIR "RENAMED-REF"

#define GIT_SYMREF "ref: "
#define GIT_PACKEDREFS_FILE "packed-refs"
#define GIT_PACKEDREFS_HEADER "# pack-refs with: peeled "
#define GIT_PACKEDREFS_FILE_MODE 0666

#define GIT_HEAD_FILE "HEAD"
#define GIT_FETCH_HEAD_FILE "FETCH_HEAD"
#define GIT_MERGE_HEAD_FILE "MERGE_HEAD"
#define GIT_REFS_HEADS_MASTER_FILE GIT_REFS_HEADS_DIR "master"

#define GIT_REFNAME_MAX 1024

struct git_reference {
	unsigned int flags;
	git_repository *owner;
	char *name;
	time_t mtime;

	union {
		git_oid oid;
		char *symbolic;
	} target;
};

typedef struct {
	git_strmap *packfile;
	time_t packfile_time;
} git_refcache;

void git_repository__refcache_free(git_refcache *refs);

int git_reference__normalize_name(char *buffer_out, size_t out_size, const char *name);
int git_reference__normalize_name_oid(char *buffer_out, size_t out_size, const char *name);

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

#endif
