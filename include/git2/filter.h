/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_filter_h__
#define INCLUDE_git_filter_h__

#include "common.h"
#include "types.h"

/**
 * @file git2/filter.h
 * @brief Allow applying filters on blobs.
 * @defgroup git_filter Allow applying filters on blobs.
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * Direction of the filter to apply.
 *
 * `GIT_FILTER_TO_WORKTREE` means applying filters from the odb to the worktree.
 *
 * `GIT_FILTER_TO_ODB` means applying filters from the worktree to the odb.
 */
typedef enum {
	GIT_FILTER_TO_WORKTREE = 1,
	GIT_FILTER_TO_ODB = 2
} git_filter_mode_t;

/**
 * Structure describing a filter.
 *
 * The filter must be able to know when it should apply (through the
 * `must_be_applied()` function), and must be able to filter both from
 * the worktree to the odb, and from the odb to the worktree.
 *
 * - `must_be_applied` must return 0 if the filter shouldn't be applied
 *   for the passed path and the passed direction (to odb or to worktree).
 * - `apply_to_odb` must apply the filter to the passed `source`, and store the
 *   result in `dst`. `source` may contain NUL characters , so the filter should
 *   rely on the `source_size` to get the real length of the content to filter.
 *   The filter must also store the filtered content length in `dst_size`.
 * - `apply_to_worktree` must apply the filter to the passed `source`, and store the
 *   result in `dst`. `source` may contain NUL characters , so the filter should
 *   rely on the `source_size` to get the real length of the content to filter.
 *   The filter must also store the filtered content length in `dst_size`.
 * - `do_free` free filter-specific resources. Called when freeing a repository
 *   through `git_repository_free`
 */
typedef struct git_filter {
	int (*should_apply_to_path)(struct git_filter *self, git_repository *repo, const char *path, git_filter_mode_t mode);
	int (*apply_to_odb)(struct git_filter *self, git_repository *repo, const char *path, const char *source, size_t source_size, char **dst, size_t *dst_size);
	int (*apply_to_worktree)(struct git_filter *self, git_repository *repo, const char *path, const char *source, size_t source_size, char **dst, size_t *dst_size);
	void (*do_free)(struct git_filter *self);
} git_filter;

/** @} */
GIT_END_DECL
#endif
