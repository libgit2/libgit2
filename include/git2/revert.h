/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_revert_h__
#define INCLUDE_git_revert_h__

#include "common.h"
#include "types.h"
#include "merge.h"

/**
 * @file git2/revert.h
 * @brief Git revert routines
 * @defgroup git_revert Git revert routines
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

typedef struct {
	unsigned int version;

	/** For merge commits, the "mainline" is treated as the parent. */
	unsigned int mainline;

	git_merge_tree_opts merge_tree_opts;
	git_checkout_opts checkout_opts;
} git_revert_opts;

#define GIT_REVERT_OPTS_VERSION 1
#define GIT_REVERT_OPTS_INIT {GIT_REVERT_OPTS_VERSION, 0, GIT_MERGE_TREE_OPTS_INIT, GIT_CHECKOUT_OPTS_INIT}

/**
* Reverts the given commits, producing changes in the working directory.
*
* @param repo the repository to revert
* @param commits the commits to revert
* @param commits_len the number of commits to revert
* @param flags merge flags
*/
GIT_EXTERN(int) git_revert(
	git_repository *repo,
	git_commit *commit,
	const git_revert_opts *given_opts);

/** @} */
GIT_END_DECL
#endif

