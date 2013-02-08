/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_pathspec_h__
#define INCLUDE_pathspec_h__

#include "common.h"
#include "buffer.h"
#include "vector.h"
#include "pool.h"

/* what is the common non-wildcard prefix for all items in the pathspec */
extern char *git_pathspec_prefix(const git_strarray *pathspec);

/* is there anything in the spec that needs to be filtered on */
extern bool git_pathspec_is_interesting(const git_strarray *pathspec);

/* build a vector of fnmatch patterns to evaluate efficiently */
extern int git_pathspec_init(
	git_vector *vspec, const git_strarray *strspec, git_pool *strpool);

/* free data from the pathspec vector */
extern void git_pathspec_free(git_vector *vspec);

/*
 * Match a path against the vectorized pathspec.
 * The matched pathspec is passed back into the `matched_pathspec` parameter,
 * unless it is passed as NULL by the caller.
 */
extern bool git_pathspec_match_path(
	git_vector *vspec,
	const char *path,
	bool disable_fnmatch,
	bool casefold,
	const char **matched_pathspec);

#endif
