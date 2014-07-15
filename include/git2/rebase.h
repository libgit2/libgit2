/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_rebase_h__
#define INCLUDE_git_rebase_h__

#include "common.h"
#include "types.h"
#include "oid.h"

/**
 * @file git2/rebase.h
 * @brief Git rebase routines
 * @defgroup git_rebase Git merge routines
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

typedef struct {
	unsigned int version;

	/**
	 * Provide a quiet rebase experience; unused by libgit2 but provided for
	 * interoperability with other clients.
	 */
	int quiet;
} git_rebase_options;

#define GIT_REBASE_OPTIONS_VERSION 1
#define GIT_REBASE_OPTIONS_INIT {GIT_REBASE_OPTIONS_VERSION}

/**
 * Initializes a `git_rebase_options` with default values. Equivalent to
 * creating an instance with GIT_REBASE_OPTIONS_INIT.
 *
 * @param opts the `git_rebase_options` instance to initialize.
 * @param version the version of the struct; you should pass
 *        `GIT_REBASE_OPTIONS_VERSION` here.
 * @return Zero on success; -1 on failure.
 */
GIT_EXTERN(int) git_rebase_init_options(
	git_rebase_options *opts,
	unsigned int version);

/**
 * Sets up a rebase operation to rebase the changes in ours relative to
 * upstream onto another branch.
 *
 * @param repo The repository to perform the rebase
 * @param branch The terminal commit to rebase
 * @param upstream The commit to begin rebasing from, or NULL to rebase all
 *                 reachable commits
 * @param onto The branch to rebase onto, or NULL to rebase onto the given
 *             upstream
 * @param signature The signature of the rebaser
 * @param opts Options to specify how rebase is performed
 * @return Zero on success; -1 on failure.
 */
GIT_EXTERN(int) git_rebase(
	git_repository *repo,
	const git_merge_head *branch,
	const git_merge_head *upstream,
	const git_merge_head *onto,
	const git_signature *signature,
	const git_rebase_options *opts);

/**
 * Applies the next patch, updating the index and working directory with the
 * changes.  If there are conflicts, you will need to address those before
 * committing the changes.
 *
 * @param repo The repository with a rebase in progress
 * @param checkout_opts Options to specify how the patch should be checked out
 * @return Zero on success; -1 on failure.
 */
GIT_EXTERN(int) git_rebase_next(
	git_repository *repo,
	git_checkout_options *checkout_opts);

/**
 * Aborts a rebase that is currently in progress, resetting the repository
 * and working directory to their state before rebase began.
 *
 * @param repo The repository with the in-progress rebase
 * @param signature The identity that is aborting the rebase
 * @return Zero on success; GIT_ENOTFOUND if a rebase is not in progress,
 *         -1 on other errors.
 */
GIT_EXTERN(int) git_rebase_abort(
	git_repository *repo,
	const git_signature *signature);

/** @} */
GIT_END_DECL
#endif
