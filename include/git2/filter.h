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
 * `GIT_FILTER_TO_WORKDIR` means applying filters from the odb to the workdir.
 *
 * `GIT_FILTER_TO_ODB` means applying filters from the workdir to the odb.
 */
typedef enum {
	GIT_FILTER_TO_WORKDIR = 1,
	GIT_FILTER_TO_ODB = 2
} git_filter_mode_t;

/**
 * Used to determine if a given filter must be applied to the
 * given path, and for the given direction (mode).
 *
 * If the function returns 0, the filter won't be applied.
 * if this function returns any other value, the filter will be applied.
 */
typedef int (*should_apply_to_path_cb)(
	struct git_filter *self,
	git_repository *repo,
	const char *path,
	git_filter_mode_t mode);

/**
 * Used to apply a filter on a given source.
 *
 * The function must apply the filter to the passed `source`, and store the
 * result in `dst`. `source` may contain NUL characters , so the filter should
 * rely on the `source_size` to get the real length of the content to filter.
 * The filter must also store the filtered content length in `dst_size`.
 */
typedef int (*apply_to_cb)(
	struct git_filter *self,
	git_repository *repo,
	const char *path,
	const char *source,
	size_t source_size,
	char **dst,
	size_t *dst_size);

/**
 * Used to free the given filter, and associated filter-specific resources.
 *
 * It will be called when the repository this filter belongs to
 * is freed  through `git_repository_free()`.
 */
typedef void (*do_free_cb)(struct git_filter *self);

/**
 * Allocate and initialize a new `git_filter`.
 * 
 * The filter must be able to know when it should apply (through the
 * `must_be_applied()` function), and must be able to filter both from
 * the workdir to the odb, and from the odb to the workdir.
 *
 * @param out the allocated `git_filter`
 * @param should_apply_to_path callback which will be called to determine if
 *   the filter must be applied or not. must return 0 if the filter shouldn't
 *   be applied for the passed path and the passed direction.
 * @param apply_to_odb callback which will be called to filter from workdir to
 *   odb. The implementation must apply the filter to the passed `source`, and
 *   store the result in `dst`. `source` may contain NUL characters , so the
 *   filter must rely on the `source_size` to get the real length of the
 *   content to filter.
 *   The filter must also store the filtered content length in `dst_size`.
 * @param apply_to_workdir callback which will be called to filter from odb to
 *   workdir. The implementation must apply the filter to the passed `source`,
 *   and store the result in `dst`. `source` may contain NUL characters , so
 *   the filter must rely on the `source_size` to get the real length of the
 *   content to filter.
 *   The filter must also store the filtered content length in `dst_size`.
 * @param do_free callback called to free filter-specific resources. Called
 *   when freeing a repository through `git_repository_free`.
 * @param name the name of the filter
 */
int git_filter_create_filter(
	git_filter **out,
	should_apply_to_path_cb should_apply, 
	apply_to_cb apply_to_odb,
	apply_to_cb apply_to_workdir,
	do_free_cb free,
	const char *name);

/**
 * Free an existing `git_filter` object.
 *
 * @param filter an existing `git_filter` object
 */
GIT_EXTERN(void) git_filter_free(
	git_filter *filter);

/** @} */
GIT_END_DECL
#endif
