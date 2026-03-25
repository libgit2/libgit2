/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_bundle_h__
#define INCLUDE_git_bundle_h__

#include "common.h"
#include "net.h"
#include "oidarray.h"
#include "strarray.h"
#include "types.h"

/**
 * @file git2/bundle.h
 * @brief Git bundle support: create and read bundle files
 * @defgroup git_bundle Bundle file support
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/** Opaque handle for a parsed git bundle */
typedef struct git_bundle git_bundle;

/**
 * Check whether a file looks like a valid git bundle without fully parsing it.
 * Only the first line (the signature) is inspected.
 *
 * @param path Path to the file to inspect
 * @return 1 if the file has a valid bundle signature, 0 if not,
 *         or a negative error code on I/O failure
 */
GIT_EXTERN(int) git_bundle_is_valid(const char *path);

/**
 * Open and parse a git bundle file.
 *
 * Reads and validates the bundle header (signature, capabilities,
 * prerequisites, and references). The packfile data is not read; the
 * offset is recorded so the transport can seek to it on demand.
 *
 * @param out Receives the newly allocated bundle handle on success
 * @param path Path to the bundle file
 * @return 0 on success, GIT_ENOTFOUND if the file does not exist,
 *         GIT_EINVALID if the header is malformed,
 *         GIT_ENOTSUPPORTED if an unsupported capability is required,
 *         or another error code
 */
GIT_EXTERN(int) git_bundle_open(git_bundle **out, const char *path);

/**
 * Get the references advertised by the bundle.
 *
 * The returned array is owned by the bundle and remains valid until
 * the bundle is freed. Do not modify or free the entries.
 *
 * @param out Receives a pointer to the array of remote-head entries
 * @param count Receives the number of entries
 * @param bundle The bundle handle
 * @return 0 on success or an error code
 */
GIT_EXTERN(int) git_bundle_refs(
	const git_remote_head ***out,
	size_t *count,
	git_bundle *bundle);

/**
 * Get the prerequisite OIDs listed in the bundle header.
 *
 * Prerequisites are commits the recipient must already have for the
 * bundle to apply correctly. Use git_bundle_verify() to confirm they
 * are present before attempting a fetch.
 *
 * The caller must release the array with git_oidarray_dispose().
 *
 * @param out Receives the array of prerequisite OIDs
 * @param bundle The bundle handle
 * @return 0 on success or an error code
 */
GIT_EXTERN(int) git_bundle_prerequisites(git_oidarray *out, git_bundle *bundle);

/**
 * Verify that all prerequisites declared in the bundle are satisfied.
 *
 * Each prerequisite OID is looked up in `repo`'s object database. The
 * function checks that the OID exists and that its type is GIT_OBJECT_COMMIT.
 *
 * @param repo The repository to check prerequisites against
 * @param bundle The bundle handle
 * @return 0 if all prerequisites are satisfied,
 *         GIT_ENOTFOUND if any prerequisite commit is absent from the ODB,
 *         GIT_EINVALID if a prerequisite OID resolves to a non-commit object,
 *         or another error code
 */
GIT_EXTERN(int) git_bundle_verify(git_repository *repo, git_bundle *bundle);

/**
 * Options for creating a bundle file.
 */
typedef struct git_bundle_create_options {
	unsigned int version;        /**< Structure version; pass GIT_BUNDLE_CREATE_OPTIONS_VERSION */
	unsigned int bundle_version; /**< Bundle format version: 2 or 3 (default: 2) */

#ifdef GIT_EXPERIMENTAL_SHA256
	git_oid_t oid_type;          /**< OID type; only meaningful for bundle_version == 3 */
#endif
} git_bundle_create_options;

/** Current version for git_bundle_create_options */
#define GIT_BUNDLE_CREATE_OPTIONS_VERSION 1
/** Static initializer for git_bundle_create_options */
#define GIT_BUNDLE_CREATE_OPTIONS_INIT { GIT_BUNDLE_CREATE_OPTIONS_VERSION, 2 }

/**
 * Initialize a git_bundle_create_options structure with default values.
 *
 * @param opts The structure to initialize
 * @param version Pass GIT_BUNDLE_CREATE_OPTIONS_VERSION
 * @return 0 on success or an error code
 */
GIT_EXTERN(int) git_bundle_create_options_init(
	git_bundle_create_options *opts,
	unsigned int version);

/**
 * Create a git bundle file from a repository.
 *
 * The commits visited by `walk` (and all objects they reference) are
 * packed into the bundle. Use git_revwalk_push() to specify the tips to
 * include and git_revwalk_hide() to exclude history the recipient already
 * has (those excluded commits become prerequisites listed in the header).
 *
 * The `refs` array specifies which named references are advertised in the
 * bundle header; each entry must be a valid refname that resolves in
 * `repo`. If `refs` is NULL the function records all local branches and
 * tags that point to objects reachable from the walk.
 *
 * @param path Destination file path for the bundle
 * @param repo Source repository
 * @param walk A configured revwalk defining the commit range to bundle
 * @param refs References to advertise in the bundle header (may be NULL)
 * @param opts Creation options (may be NULL for defaults)
 * @return 0 on success or an error code
 */
GIT_EXTERN(int) git_bundle_create(
	const char *path,
	git_repository *repo,
	git_revwalk *walk,
	const git_strarray *refs,
	const git_bundle_create_options *opts);

/**
 * Free a git bundle handle.
 *
 * @param bundle The bundle to free (safe to call with NULL)
 */
GIT_EXTERN(void) git_bundle_free(git_bundle *bundle);

/** @} */
GIT_END_DECL

#endif
