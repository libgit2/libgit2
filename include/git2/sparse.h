/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#ifndef INCLUDE_git_sparse_h__
#define INCLUDE_git_sparse_h__

#include "common.h"
#include "checkout.h"
#include "repository.h"
#include "strarray.h"

/**
 * @file git2/sparse.h
 * @brief Sparse checkout configuration
 * @defgroup git_sparse Sparse checkout configuration
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * Configure cone-mode sparse checkout directories.
 *
 * This writes Git-compatible cone-mode patterns to
 * `$GIT_DIR/info/sparse-checkout` and enables both
 * `core.sparseCheckout` and `core.sparseCheckoutCone`.
 *
 * Directory names must be non-empty, relative paths using forward
 * slashes. The root-level files of the working tree are always
 * included, as required by cone-mode sparse checkout.
 *
 * This function only persists sparse-checkout state. Applying that
 * state to the index and working directory is performed separately.
 *
 * @param repo repository to configure; must not be bare
 * @param directories directories to include in the cone
 * @return 0 on success, or an error code
 */
GIT_EXTERN(int) git_sparse_checkout_set(
	git_repository *repo,
	const git_strarray *directories);

/**
 * Update index skip-worktree flags from sparse-checkout patterns.
 *
 * This reads the cone-mode patterns in `$GIT_DIR/info/sparse-checkout`
 * and marks excluded stage-zero index entries with
 * `GIT_INDEX_ENTRY_SKIP_WORKTREE`. Included entries have that flag
 * cleared.
 *
 * This function does not add, remove, or otherwise modify files in the
 * working directory.
 *
 * @param repo repository whose index will be updated
 * @return 0 on success, or an error code
 */
GIT_EXTERN(int) git_sparse_checkout_update_index(git_repository *repo);

/**
 * Initialize an empty index from HEAD and apply sparse-checkout flags.
 *
 * This is intended for repositories cloned with `GIT_CHECKOUT_NONE`.
 * The index must be empty; an existing index is left untouched to avoid
 * discarding staged changes.
 *
 * This function does not modify the working directory.
 *
 * @param repo repository whose empty index will be initialized
 * @return 0 on success, GIT_EUNCOMMITTED if the index is non-empty,
 *         or an error code
 */
GIT_EXTERN(int) git_sparse_checkout_initialize_index(git_repository *repo);

/**
 * Materialize included sparse-checkout paths from HEAD.
 *
 * The repository index must already have been initialized with
 * `git_sparse_checkout_initialize_index`. Only stage-zero entries
 * without `GIT_INDEX_ENTRY_SKIP_WORKTREE` are checked out.
 *
 * Existing excluded files are not removed from the working directory.
 *
 * @param repo repository whose sparse paths will be checked out
 * @param opts optional checkout options; callbacks are preserved while
 *        sparse checkout controls the strategy, baseline, and paths
 * @return 0 on success, or an error code
 */
GIT_EXTERN(int) git_sparse_checkout_checkout(
	git_repository *repo,
	const git_checkout_options *opts);

/**
 * Configure and materialize a cone-mode sparse checkout.
 *
 * This writes the sparse-checkout configuration, initializes an empty
 * index from HEAD or updates an existing index, and materializes the
 * included paths. Conflicted indexes and indexes with staged changes are
 * rejected. An empty index is initialized from HEAD.
 *
 * Clean paths that become excluded are removed from the working directory;
 * locally modified excluded paths are preserved.
 *
 * @param repo repository to configure and check out
 * @param directories directories to include in the cone
 * @param opts optional checkout options passed to materialization
 * @return 0 on success, GIT_ECONFLICT for an unmerged index,
 *         GIT_EUNCOMMITTED for staged changes, or an error code
 */
GIT_EXTERN(int) git_sparse_checkout_apply(
	git_repository *repo,
	const git_strarray *directories,
	const git_checkout_options *opts);

/** Disable sparse checkout and materialize all HEAD paths. */
GIT_EXTERN(int) git_sparse_checkout_disable(
	git_repository *repo,
	const git_checkout_options *opts);

/** List explicitly selected cone-mode sparse checkout directories. */
GIT_EXTERN(int) git_sparse_checkout_list(
	git_strarray *out,
	git_repository *repo);

/** Determine whether cone-mode sparse checkout is enabled. */
GIT_EXTERN(int) git_sparse_checkout_is_enabled(
	int *out,
	git_repository *repo);

/** @} */
GIT_END_DECL

#endif
