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

typedef struct git_graph_commit_list git_graph_commit_list;

/**
 * Get the ahead/behind list of commits.
 *
 * There is no need for branches containing the commits to have any
 * upstream relationship, but it helps to think of one as a branch and
 * the other as its upstream, the `ahead` and `behind` values will be
 * what git would report for the branches.
 *
 * @param ahead list of unique commits in `upstream`
 * @param behind list of unique commits in `local`
 * @param repo the repository where the commits exist
 * @param local the commit for local
 * @param upstream the commit for upstream
 */
GIT_EXTERN(int) git_graph_ahead_behind(
	git_graph_commit_list **ahead,
	git_graph_commit_list **behind,
	git_repository *repo,
	const git_oid *local,
	const git_oid *upstream);

/**
 * Get the count of elements in the list.
 *
 * @param list list from which to get the count
 */
GIT_EXTERN(int) git_graph_commit_list_count(git_graph_commit_list *list);

/**
 * Get the nth commit of the list.
 *
 * @param list list from which to get the nth commit
 * @param pos the position of the commit
 * @return a pointer to a git_oid; NULL if out of bounds
 */
GIT_EXTERN(const git_oid *) git_graph_commit_list_get_byindex(
	git_graph_commit_list *list,
	size_t pos);

/**
 * Free an existing git_graph_commit_list object.
 *
 * @param list an existing git_graph_commit_list object
 */
GIT_EXTERN(void) git_graph_commit_list_free(git_graph_commit_list *list);

/** @} */
GIT_END_DECL
#endif
