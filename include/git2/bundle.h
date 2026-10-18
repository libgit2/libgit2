/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_bundle_h__
#define INCLUDE_git_bundle_h__

#include "common.h"
#include "types.h"
#include "strarray.h"
#include "pack.h"

/**
 * @file git2/bundle.h
 * @brief Create Git bundle files
 * @defgroup git_bundle Create Git bundle files
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * Bundle creation options structure
 *
 * Initialize with `GIT_BUNDLE_CREATE_OPTIONS_INIT`. Alternatively, you can
 * use `git_bundle_create_options_init`.
 *
 * @options[version] GIT_BUNDLE_CREATE_OPTIONS_VERSION
 * @options[init_macro] GIT_BUNDLE_CREATE_OPTIONS_INIT
 * @options[init_function] git_bundle_create_options_init
 */
typedef struct git_bundle_create_options {
	unsigned int version;

	/**
	 * Progress callback, called while objects are collected
	 * (`GIT_PACKBUILDER_ADDING_OBJECTS`) and while deltas are computed
	 * (`GIT_PACKBUILDER_DELTAFICATION`).  Calls are rate-limited, and
	 * none are made while the pack is written.  It is only called
	 * during `git_bundle_create`.  Return 0 to continue or a negative
	 * value to cancel; that value is returned by `git_bundle_create`.
	 */
	git_packbuilder_progress progress_cb;

	/** Payload passed to `progress_cb` */
	void *progress_cb_payload;
} git_bundle_create_options;

/** Current version for the `git_bundle_create_options` structure */
#define GIT_BUNDLE_CREATE_OPTIONS_VERSION 1

/** Static constructor for `git_bundle_create_options` */
#define GIT_BUNDLE_CREATE_OPTIONS_INIT { GIT_BUNDLE_CREATE_OPTIONS_VERSION }

/**
 * Initialize git_bundle_create_options structure
 *
 * Initializes a `git_bundle_create_options` with default values. Equivalent
 * to creating an instance with `GIT_BUNDLE_CREATE_OPTIONS_INIT`.
 *
 * @param opts The `git_bundle_create_options` struct to initialize.
 * @param version The struct version; pass `GIT_BUNDLE_CREATE_OPTIONS_VERSION`.
 * @return Zero on success; -1 on failure.
 */
GIT_EXTERN(int) git_bundle_create_options_init(
	git_bundle_create_options *opts,
	unsigned int version);

/**
 * Create a bundle file containing the given references
 *
 * The bundle is self-contained: it has no prerequisites, and its pack
 * holds every object reachable from the references it advertises.  It
 * can be cloned or fetched from by Git or libgit2.
 *
 * Each entry of `refs` must be a full reference name (`refs/...`) or
 * `HEAD`; shorthand such as `main` is rejected.  All references are
 * resolved once, before any output is created; a symbolic reference is
 * advertised under the given name with its target's id.  A reference
 * that is missing, unborn, or cannot be resolved is an error.  `HEAD`
 * is advertised only when it is selected.
 *
 * Entries are advertised in the order given; later duplicates are
 * dropped.  An annotated tag is advertised by the id of the tag object,
 * not the object it points to.  References to trees and blobs are
 * supported, as in Git; clone can only check out a `HEAD` that is a
 * commit.
 *
 * A SHA-1 repository produces a v2 bundle; a SHA-256 repository produces
 * a v3 bundle with the `object-format=sha256` capability.  Shallow
 * repositories are not supported.  Creation fails if a reachable object
 * is missing from the repository.
 *
 * Destination symlinks are followed.  The bundle is written to a lock
 * file at the resolved destination path with `.lock` appended, then
 * renamed over that destination once the whole bundle has been written.
 * The parent directory must exist.  If the lock file already exists,
 * this returns `GIT_ELOCKED` and leaves it alone.  On any failure an
 * existing destination file is left intact and no lock file created by
 * this call remains.
 *
 * The pack is written to the file as it is generated, but the list of
 * objects and the computed deltas are held in memory first, so memory
 * use grows with the size of the bundle.
 *
 * Example usage:
 *
 *     char *names[] = { "HEAD", "refs/heads/main" };
 *     git_strarray refs = { names, 2 };
 *     git_repository *clone;
 *
 *     error = git_bundle_create(repo, "repo.bundle", &refs, NULL);
 *
 *     if (!error)
 *         error = git_clone(&clone, "repo.bundle", "clone", NULL);
 *
 * @param repo the repository to read references and objects from
 * @param path the path of the bundle file to write
 * @param refs the names of the references to advertise; must not be empty
 * @param opts options for creation, or `NULL` for defaults
 * @return 0 on success, `GIT_EINVALIDSPEC` for a name that is not
 *         `HEAD` or a valid full reference name, `GIT_ENOTFOUND` for a
 *         missing or unborn reference, `GIT_ELOCKED` if the lock file
 *         exists, `GIT_ENOTSUPPORTED` for a shallow repository, the
 *         callback's value if it cancels, or another error code
 */
GIT_EXTERN(int) git_bundle_create(
	git_repository *repo,
	const char *path,
	const git_strarray *refs,
	const git_bundle_create_options *opts);

/** @} */
GIT_END_DECL

#endif
