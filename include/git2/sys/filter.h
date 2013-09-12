/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_sys_git_filter_h__
#define INCLUDE_sys_git_filter_h__

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
 * Look up a filter by name
 *
 * @param name The name of the filter
 * @return Pointer to the filter object or NULL if not found
 */
GIT_EXTERN(git_filter *) git_filter_lookup(const char *name);

#define GIT_FILTER_CRLF  "crlf"
#define GIT_FILTER_IDENT "ident"

#define GIT_FILTER_CRLF_PRIORITY 0
#define GIT_FILTER_IDENT_PRIORITY 100

/**
 * Create a new empty filter list
 *
 * Normally you won't use this because `git_filter_list_load` will create
 * the filter list for you, but you can use this in combination with the
 * `git_filter_lookup` and `git_filter_list_push` functions to assemble
 * your own chains of filters.
 */
GIT_EXTERN(int) git_filter_list_new(
	git_filter_list **out, git_repository *repo, git_filter_mode_t mode);

/**
 * Add a filter to a filter list with the given payload.
 *
 * Normally you won't have to do this because the filter list is created
 * by calling the "check" function on registered filters when the filter
 * attributes are set, but this does allow more direct manipulation of
 * filter lists when desired.
 *
 * Note that normally the "check" function can set up a payload for the
 * filter.  Using this function, you can either pass in a payload if you
 * know the expected payload format, or you can pass NULL.  Some filters
 * may fail with a NULL payload.  Good luck!
 */
GIT_EXTERN(int) git_filter_list_push(
	git_filter_list *fl, git_filter *filter, void *payload);

/**
 * Look up how many filters are in the list
 *
 * We will attempt to apply all of these filters to any data passed in,
 * but note that the filter apply action still has the option of skipping
 * data that is passed in (for example, the CRLF filter will skip data
 * that appears to be binary).
 *
 * @param fl A filter list
 * @return The number of filters in the list
 */
GIT_EXTERN(size_t) git_filter_list_length(const git_filter_list *fl);

/**
 * A filter source represents a file/blob to be processed
 */
typedef struct git_filter_source git_filter_source;

/**
 * Get the repository that the source data is coming from.
 */
GIT_EXTERN(git_repository *) git_filter_source_repo(const git_filter_source *src);

/**
 * Get the path that the source data is coming from.
 */
GIT_EXTERN(const char *) git_filter_source_path(const git_filter_source *src);

/**
 * Get the file mode of the source file
 * If the mode is unknown, this will return 0
 */
GIT_EXTERN(uint16_t) git_filter_source_filemode(const git_filter_source *src);

/**
 * Get the OID of the source
 * If the OID is unknown (often the case with GIT_FILTER_CLEAN) then
 * this will return NULL.
 */
GIT_EXTERN(const git_oid *) git_filter_source_id(const git_filter_source *src);

/**
 * Get the git_filter_mode_t to be applied
 */
GIT_EXTERN(git_filter_mode_t) git_filter_source_mode(const git_filter_source *src);

/*
 * struct git_filter
 *
 * The filter lifecycle:
 * - initialize - first use of filter
 * - shutdown   - filter removed/unregistered from system
 * - check      - considering for file
 * - apply      - applied to file
 * - cleanup    - done with file
 */

/**
 * Initialize callback on filter
 */
typedef int (*git_filter_init_fn)(git_filter *self);

/**
 * Shutdown callback on filter
 */
typedef void (*git_filter_shutdown_fn)(git_filter *self);

/**
 * Callback to decide if a given source needs this filter
 */
typedef int (*git_filter_check_fn)(
	git_filter  *self,
	void       **payload, /* points to NULL ptr on entry, may be set */
	const git_filter_source *src,
	const char **attr_values);

/**
 * Callback to actually perform the data filtering
 */
typedef int (*git_filter_apply_fn)(
	git_filter    *self,
	void         **payload, /* may be read and/or set */
	git_buf       *to,
	const git_buf *from,
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
 * `attributes` is a whitespace-separated list of attribute names to check
 * for this filter (e.g. "eol crlf text").  If the attribute name is bare,
 * it will be simply loaded and passed to the `check` callback.  If it has
 * a value (i.e. "name=value"), the attribute must match that value for
 * the filter to be applied.
 *
 * `initialize` is an optional callback invoked before a filter is first
 * used.  It will be called once at most.
 *
 * `shutdown` is an optional callback invoked when the filter is
 * unregistered or when libgit2 is shutting down.  It will be called once
 * at most and should free any memory as needed.
 *
 * `check` is an optional callback that checks if filtering is needed for
 * a given source.  It should return 0 if the filter should be applied
 * (i.e. success), GIT_ENOTFOUND if the filter should not be applied, or
 * an other error code to fail out of the filter processing pipeline and
 * return to the caller.
 *
 * `apply` is the callback that actually filters data.  If it successfully
 * writes the output, it should return 0.  Like `check`, it can return
 * GIT_ENOTFOUND to indicate that the filter doesn't actually want to run.
 * Other error codes will stop filter processing and return to the caller.
 *
 * `cleanup` is an optional callback that is made after the filter has
 * been applied.  Both the `check` and `apply` callbacks are able to
 * allocate a `payload` to keep per-source filter state, and this callback
 * is given that value and can clean up as needed.
 */
struct git_filter {
	unsigned int           version;

	const char            *attributes;

	git_filter_init_fn     initialize;
	git_filter_shutdown_fn shutdown;
	git_filter_check_fn    check;
	git_filter_apply_fn    apply;
	git_filter_cleanup_fn  cleanup;
};

#define GIT_FILTER_VERSION 1

/**
 * Register a filter under a given name with a given priority.
 *
 * If non-NULL, the filter's initialize callback will be invoked before
 * the first use of the filter, so you can defer expensive operations (in
 * case libgit2 is being used in a way that doesn't need the filter).
 *
 * A filter's attribute checks and `check` and `apply` callbacks will be
 * issued in order of `priority` on smudge (to workdir), and in reverse
 * order of `priority` on clean (to odb).
 *
 * Two filters are preregistered with libgit2:
 * - GIT_FILTER_CRLF with priority 0
 * - GIT_FILTER_IDENT with priority 100
 *
 * Currently the filter registry is not thread safe, so any registering or
 * deregistering of filters must be done outside of any possible usage of
 * the filters (i.e. during application setup or shutdown).
 */
GIT_EXTERN(int) git_filter_register(
	const char *name, git_filter *filter, int priority);

/**
 * Remove the filter with the given name
 *
 * It is not allowed to remove the builtin libgit2 filters.
 *
 * Currently the filter registry is not thread safe, so any registering or
 * deregistering of filters must be done outside of any possible usage of
 * the filters (i.e. during application setup or shutdown).
 */
GIT_EXTERN(int) git_filter_unregister(const char *name);

/** @} */
GIT_END_DECL
#endif
