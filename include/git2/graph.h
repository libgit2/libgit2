/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_graph_h__
#define INCLUDE_git_graph_h__

#include "common.h"
#include "types.h"
#include "oid.h"

/**
 * @file git2/graph.h
 * @brief Git graph traversal routines
 * @defgroup git_revwalk Git graph traversal routines
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * Count the number of unique commits between two commit objects
 *
 * @param ahead_out number of commits, starting at `one`, unique from commits
 *			in `two`
 * @param behind_out number of commits, starting at `two`, unique from commits
 *			in `one`
 * @param common_ancestor_out oid of the found common ancestor; Zero oid if no
 *			common ancestor has been found. Use `git_oid_iszero()` to check
 *			the content of the oid.
 * @param repo the repository where the commits exist
 * @param one one of the commits
 * @param two the other commit
 * @return 0 on success or -1 on error
 */
GIT_EXTERN(int) git_graph_ahead_behind(
	size_t *ahead_out,
	size_t *behind_out,
	git_oid *common_ancestor_out,
	git_repository *repo,
	const git_oid *one,
	const git_oid *two);

/** @} */
GIT_END_DECL
#endif
