/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_sys_git_specialrefdb_backend_h__
#define INCLUDE_sys_git_specialrefdb_backend_h__

#include "git2/common.h"
#include "git2/types.h"
#include "git2/oid.h"

/**
 * @file git2/sys/specialrefdb_backend.h
 * @brief Custom database backends for special reference storage
 * @defgroup git_refdb_backend Custom database backends for special reference storage
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/** An instance for a custom backend */
struct git_specialrefdb_backend {
	unsigned int version; /**< The backend API version */

	/**
	 * Looks up the HEAD special reference.
	 *
	 * A refdb implementation must provide this function.
	 *
	 * @param out The implementation shall set this to the allocated
	 *            reference, if it could be found.
	 * @param backend The special reference db backend.
	 *                existence.
	 * @return `0` on success, or an error code.
	 */
	int GIT_CALLBACK(lookup_head)(
		git_reference **out,
		git_specialrefdb_backend *backend);

	/**
	 * Frees any resources held bj the special reference db (including the
	 * `git_specialrefdb_backend` itself).
	 *
	 * A special ref db backend implementation must provide this function.
	 */
	void GIT_CALLBACK(free)(git_specialrefdb_backend *backend);
};

/** Current version for the `git_specialrefdb_backend_options` structure */
#define GIT_SPECIALREFDB_BACKEND_VERSION 1

/** Static constructor for `git_specialrefdb_backend_options` */
#define GIT_SPECIALREFDB_BACKEND_INIT {GIT_SPECIALREFDB_BACKEND_VERSION}

/**
 * Constructors for default git-compatible special refdb backend
 *
 * Under normal usage, this is called for you when the repository is
 * opened / created, but you can use this to explicitly construct a
 * filesystem refdb backend for a repository.
 *
 * @param backend_out Output pointer to the git_specialrefdb_backend object
 * @param repo Git repository to access
 * @return 0 on success, or an error code on failure
 */
GIT_EXTERN(int) git_specialrefdb_git(
	git_specialrefdb_backend **backend_out,
	git_repository *repo);

/** @} */
GIT_END_DECL

#endif
