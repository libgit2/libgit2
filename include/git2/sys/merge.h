/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_sys_git_merge_h__
#define INCLUDE_sys_git_merge_h__

/**
 * @file git2/sys/merge.h
 * @brief Git merge driver backend and plugin routines
 * @defgroup git_backend Git custom backend APIs
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

typedef struct git_merge_driver git_merge_driver;

/**
 * Look up a merge driver by name
 *
 * @param name The name of the merge driver
 * @return Pointer to the merge driver object or NULL if not found
 */
GIT_EXTERN(git_merge_driver *) git_merge_driver_lookup(const char *name);

#define GIT_MERGE_DRIVER_TEXT   "text"
#define GIT_MERGE_DRIVER_BINARY "binary"
#define GIT_MERGE_DRIVER_UNION  "union"

/**
 * A merge driver source represents the file to be merged
 */
typedef struct git_merge_driver_source git_merge_driver_source;

/** Get the repository that the source data is coming from. */
GIT_EXTERN(git_repository *) git_merge_driver_source_repo(
	const git_merge_driver_source *src);

/** Gets the ancestor of the file to merge. */
GIT_EXTERN(git_index_entry *) git_merge_driver_source_ancestor(
	const git_merge_driver_source *src);

/** Gets the ours side of the file to merge. */
GIT_EXTERN(git_index_entry *) git_merge_driver_source_ours(
	const git_merge_driver_source *src);

/** Gets the theirs side of the file to merge. */
GIT_EXTERN(git_index_entry *) git_merge_driver_source_theirs(
	const git_merge_driver_source *src);

/** Gets the merge file options that the merge was invoked with */
GIT_EXTERN(git_merge_file_options *) git_merge_driver_source_file_options(
	const git_merge_driver_source *src);


/*
 * struct git_merge_driver
 *
 * The merge driver lifecycle:
 * - initialize - first use of merge driver
 * - shutdown   - merge driver removed/unregistered from system
 * - check      - considering using merge driver for file
 * - apply      - apply merge driver to the file
 * - cleanup    - done with file
 */

/**
 * Initialize callback on merge driver
 *
 * Specified as `driver.initialize`, this is an optional callback invoked
 * before a merge driver is first used.  It will be called once at most.
 *
 * If non-NULL, the merge driver's `initialize` callback will be invoked
 * right before the first use of the driver, so you can defer expensive
 * initialization operations (in case libgit2 is being used in a way that
 * doesn't need the merge driver).
 */
typedef int (*git_merge_driver_init_fn)(git_merge_driver *self);

/**
 * Shutdown callback on merge driver
 *
 * Specified as `driver.shutdown`, this is an optional callback invoked
 * when the merge driver is unregistered or when libgit2 is shutting down.
 * It will be called once at most and should release resources as needed.
 * This may be called even if the `initialize` callback was not made.
 *
 * Typically this function will free the `git_merge_driver` object itself.
 */
typedef void (*git_merge_driver_shutdown_fn)(git_merge_driver *self);

/**
 * Callback to decide if a given conflict can be resolved with this merge
 * driver.
 *
 * Specified as `driver.check`, this is an optional callback that checks
 * if the given conflict can be resolved with this merge driver.
 *
 * It should return 0 if the merge driver should be applied (i.e. success),
 * `GIT_PASSTHROUGH` if the driver is not available, which is the equivalent
 * of an unregistered or nonexistent merge driver.  In this case, the default
 * (`text`) driver will be run.  This is useful if you register a wildcard
 * merge driver but are not interested in handling the requested file (and
 * should just fallback).  The driver can also return `GIT_EMERGECONFLICT`
 * if the driver is not able to produce a merge result, and the file will
 * remain conflicted.  Any other errors will fail and return to the caller.
 *
 * The `name` will be set to the name of the driver as configured in the
 * attributes.
 *
 * The `src` contains the data about the file to be merged.
 *
 * The `payload` will be a pointer to a reference payload for the driver.
 * This will start as NULL, but `check` can assign to this pointer for
 * later use by the `apply` callback.  Note that the value should be heap
 * allocated (not stack), so that it doesn't go away before the `apply`
 * callback can use it.  If a driver allocates and assigns a value to the
 * `payload`, it will need a `cleanup` callback to free the payload.
 */
typedef int (*git_merge_driver_check_fn)(
	git_merge_driver *self,
	void **payload,
	const char *name,
	const git_merge_driver_source *src);

/**
 * Callback to actually perform the merge.
 *
 * Specified as `driver.apply`, this is the callback that actually does the
 * merge.  If it can successfully perform a merge, it should populate
 * `path_out` with a pointer to the filename to accept, `mode_out` with
 * the resultant mode, and `merged_out` with the buffer of the merged file
 * and then return 0.  If the driver returns `GIT_PASSTHROUGH`, then the
 * default merge driver should instead be run.  It can also return
 * `GIT_EMERGECONFLICT` if the driver is not able to produce a merge result,
 * and the file will remain conflicted.  Any other errors will fail and
 * return to the caller.
 *
 * The `src` contains the data about the file to be merged.
 *
 * The `payload` value will refer to any payload that was set by the
 * `check` callback.  It may be read from or written to as needed.
 */
typedef int (*git_merge_driver_apply_fn)(
	git_merge_driver *self,
	void **payload,
	const char **path_out,
	uint32_t *mode_out,
	git_buf *merged_out,
	const git_merge_driver_source *src);

/**
 * Callback to clean up after merge has been performed.
 *
 * Specified as `driver.cleanup`, this is an optional callback invoked
 * after the driver has been run.  If the `check` or `apply` callbacks
 * allocated a `payload` to keep per-source merge driver state, use this
 * callback to free that payload and release resources as required.
 */
typedef void (*git_merge_driver_cleanup_fn)(
	git_merge_driver *self,
	void *payload);

/**
 * Merge driver structure used to register custom merge drivers.
 *
 * To associate extra data with a driver, allocate extra data and put the
 * `git_merge_driver` struct at the start of your data buffer, then cast
 * the `self` pointer to your larger structure when your callback is invoked.
 *
 * `version` should be set to GIT_MERGE_DRIVER_VERSION
 *
 * The `initialize`, `shutdown`, `check`, `apply`, and `cleanup`
 * callbacks are all documented above with the respective function pointer
 * typedefs.
 */
struct git_merge_driver {
	unsigned int                 version;

	git_merge_driver_init_fn     initialize;
	git_merge_driver_shutdown_fn shutdown;
	git_merge_driver_check_fn    check;
	git_merge_driver_apply_fn    apply;
	git_merge_driver_cleanup_fn  cleanup;
};

#define GIT_MERGE_DRIVER_VERSION 1

/**
 * Register a merge driver under a given name.
 *
 * As mentioned elsewhere, the initialize callback will not be invoked
 * immediately.  It is deferred until the driver is used in some way.
 *
 * Currently the merge driver registry is not thread safe, so any
 * registering or deregistering of merge drivers must be done outside of
 * any possible usage of the drivers (i.e. during application setup or
 * shutdown).
 *
 * @param name The name of this driver to match an attribute.  Attempting
 * 			to register with an in-use name will return GIT_EEXISTS.
 * @param driver The merge driver definition.  This pointer will be stored
 *			as is by libgit2 so it must be a durable allocation (either
 *			static or on the heap).
 * @return 0 on successful registry, error code <0 on failure
 */
GIT_EXTERN(int) git_merge_driver_register(
	const char *name, git_merge_driver *driver);

/**
 * Remove the merge driver with the given name.
 *
 * Attempting to remove the builtin libgit2 merge drivers is not permitted
 * and will return an error.
 *
 * Currently the merge driver registry is not thread safe, so any
 * registering or deregistering of drivers must be done outside of any
 * possible usage of the drivers (i.e. during application setup or shutdown).
 *
 * @param name The name under which the merge driver was registered
 * @return 0 on success, error code <0 on failure
 */
GIT_EXTERN(int) git_merge_driver_unregister(const char *name);

/** @} */
GIT_END_DECL
#endif
