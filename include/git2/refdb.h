/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_refdb_h__
#define INCLUDE_git_refdb_h__

#include "common.h"
#include "types.h"
#include "oid.h"
#include "refs.h"

/**
 * @file git2/refdb.h
 * @brief Git custom refs backend functions
 * @defgroup git_refdb Git custom refs backend API
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * Create a new reference database with no backends.
 *
 * Before the Ref DB can be used for read/writing, a custom database
 * backend must be manually set using `git_refdb_set_backend()`
 *
 * @param out location to store the database pointer, if opened.
 *			Set to NULL if the open failed.
 * @param repo the repository
 * @return 0 or an error code
 */
GIT_EXTERN(int) git_refdb_new(git_refdb **out, git_repository *repo);

/**
 * Create a new reference database and automatically add
 * the default backends:
 *
 *  - git_refdb_dir: read and write loose and packed refs
 *      from disk, assuming the repository dir as the folder
 *
 * @param out location to store the database pointer, if opened.
 *			Set to NULL if the open failed.
 * @param repo the repository
 * @return 0 or an error code
 */
GIT_EXTERN(int) git_refdb_open(git_refdb **out, git_repository *repo);

/**
 * Sets the custom backend to an existing reference DB
 *
 * Read <refdb_backends.h> for more information.
 *
 * @param refdb database to add the backend to
 * @param backend pointer to a git_refdb_backend instance
 * @param priority Value for ordering the backends queue
 * @return 0 on success; error code otherwise
 */
GIT_EXTERN(int) git_refdb_set_backend(
	git_refdb *refdb,
	git_refdb_backend *backend);

/** @} */
GIT_END_DECL

#endif
