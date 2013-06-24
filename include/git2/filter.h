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

typedef struct git_filter git_filter;

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
 */
int git_filters_create_filter(
	git_filter **out,
	should_apply_to_path_cb should_apply, 
	apply_to_cb apply_to_odb,
	apply_to_cb apply_to_workdir,
	do_free_cb free,
	const char *name);

/**
 * Register a `git_filter` in the given repository.
 *
 * This filter will be applied according to its `should_apply_to_path()`
 * definition.
 *
 * @param repo repository for which to register the filter
 * @param filter the filter to register
 * @return 0 on success, -1 on failure
 */
GIT_EXTERN(int) git_filters_register_filter(
	git_repository *repo,
	git_filter *filter);

/**
 * Remove a `git_filter` from the given repository.
 *
 * @param repo repository for which to register the filter
 * @param filtername the name of the filter to remove
 * @return 0 on success, -1 on failure
 */
GIT_EXTERN(int) git_filters_unregister_filter(
	git_repository *repo,
	const char *filtername);

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
