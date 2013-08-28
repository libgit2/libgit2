/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_sys_git_config_backend_h__
#define INCLUDE_sys_git_config_backend_h__

#include "git2/filter.h"

/**
 * @file git2/sys/filter.h
 * @brief Git filter backend and plugin routines
 * @defgroup git_backend Git custom backend APIs
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * A filter source represents a file/blob to be processed
 */
typedef struct git_filter_source git_filter_source;
struct git_filter_source {
	git_repository *repo;
	const char     *path;
	git_oid         oid;  /* zero if unknown (which is likely) */
	uint16_t        filemode; /* zero if unknown */
};

/**
 * Callback to actually perform the data filtering
 */
typedef int (*git_filter_apply_fn)(
	git_filter        *self,
	void              **payload, /* may be read and/or set */
	git_filter_mode_t mode,
	git_buffer        *to,
	const git_buffer  *from,
	const git_filter_source *src);

/**
 * Callback to decide if a given source needs this filter
 */
typedef int (*git_filter_check_fn)(
	git_filter        *self,
	void              **payload, /* points to NULL ptr on entry, may be set */
	git_filter_mode_t mode,
	const git_filter_source *src);

/**
 * Callback to clean up after filtering has been applied
 */
typedef void (*git_filter_cleanup_fn)(
	git_filter *self,
	void       *payload);

/**
 * Filter structure used to register a new filter.
 *
 * To associate extra data with a filter, simply allocate extra data
 * and put the `git_filter` struct at the start of your data buffer,
 * then cast the `self` pointer to your larger structure when your
 * callback is invoked.
 *
 * `version` should be set to GIT_FILTER_VERSION
 *
 * `apply` is the callback that actually filters data.
 *
 * `check` is an optional callback that checks if filtering is needed for
 * a given source.
 *
 * `cleanup` is an optional callback that is made after the filter has
 * been applied.  Both the `check` and `apply` callbacks are able to
 * allocate a `payload` to keep per-source filter state, and this callback
 * is given that value and can clean up as needed.
 */
struct git_filter {
	unsigned int          version;
	git_filter_apply_fn   apply;
	git_filter_check_fn   check;
	git_filter_cleanup_fn cleanup;
};

#define GIT_FILTER_VERSION 1

/**
 * Register a filter under a given name
 *
 * Two filters will be preregistered with libgit2: GIT_FILTER_CRLF and
 * GIT_FILTER_IDENT.
 */
GIT_EXTERN(int) git_filter_register(
	const char *name, const git_filter *filter);

/**
 * Remove the filter with the given name
 */
GIT_EXTERN(int) git_filter_unregister(const char *name);

/** @} */
GIT_END_DECL
#endif
