/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_sparse_h__
#define INCLUDE_git_sparse_h__

#include "common.h"
#include "types.h"

GIT_BEGIN_DECL

typedef struct {

    /**
     * Set to zero (false) to consider sparse-checkout patterns as
     * full patterns, or non-zero for cone patterns.
     */
    int cone;
} git_sparse_checkout_init_options;

#define GIT_SPARSE_CHECKOUT_INIT_OPTIONS_INIT { false };

/**
 * Enable the core.sparseCheckout setting. If the sparse-checkout
 * file does not exist, then populate it with patterns that match
 * every file in the root directory and no other directories,
 * then will remove all directories tracked by Git. Add patterns
 * to the sparse-checkout file to repopulate the working directory.
 *
 * To avoid interfering with other worktrees, it first enables the
 * extensions.worktreeConfig setting and makes sure to set the
 * core.sparseCheckout setting in the worktree-specific config file.
 *
 * @param opts The `git_sparse_checkout_init_options` when
 *      initializing the sparse-checkout file
 * @param repo Repository where to find the sparse-checkout file
 * @return 0 or an error code
 */
GIT_EXTERN(int) git_sparse_checkout_init(
        git_sparse_checkout_init_options *opts,
        git_repository *repo);

/**
 * Write a set of patterns to the sparse-checkout file.
 * Update the working directory to match the new patterns.
 * Enable the core.sparseCheckout config setting if it is not
 * already enabled.
 *
 * @param patterns Pointer to a git_strarray structure where
 *      the patterns to set can be found
 * @param repo Repository where to find the sparse-checkout file
 * @return 0 or an error code
 */
GIT_EXTERN(int) git_sparse_checkout_set(
        git_strarray *patterns,
        git_repository *repo);

/**
 * Test if the sparse-checkout rules apply to a given path.
 *
 * This function checks the sparse-checkout rules to see if they would apply to the
 * given path. This indicates if the path would be included on checkout.
 *
 * @param checkout boolean returning 1 if the sparse-checkout rules apply (the file will be checked out), 0 if they do not
 * @param repo a repository object
 * @param path the path to check sparse-checkout rules for, relative to the repo's workdir.
 * @return 0 if sparse-checkout rules could be processed for the path (regardless
 *         of whether it exists or not), or an error < 0 if they could not.
 */
GIT_EXTERN(int) git_sparse_check_path(
	int *checkout,
	git_repository *repo,
	const char *path);

GIT_END_DECL

#endif
