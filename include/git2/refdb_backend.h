/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_refdb_backend_h__
#define INCLUDE_git_refdb_backend_h__

#include "common.h"
#include "types.h"
#include "oid.h"

/**
 * @file git2/refdb_backend.h
 * @brief Git custom refs backend functions
 * @defgroup git_refdb_backend Git custom refs backend API
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/** An instance for a custom backend */
struct git_refdb_backend {
    unsigned int version;

	/**
	 * Queries the refdb backend to determine if the given ref_name
	 * exists.  A refdb implementation must provide this function.
	 */
	int (*exists)(
		int *exists,
		struct git_refdb_backend *backend,
		const char *ref_name);

	/**
	 * Queries the refdb backend for a given reference.  A refdb
	 * implementation must provide this function.
	 */
	int (*lookup)(
		git_reference **out,
		struct git_refdb_backend *backend,
		const char *ref_name);

	/**
	 * Enumerates each reference in the refdb.  A refdb implementation must
	 * provide this function.
	 */
	int (*foreach)(
		struct git_refdb_backend *backend,
		unsigned int list_flags,
		git_reference_foreach_cb callback,
		void *payload);

	/**
	 * Enumerates each reference in the refdb that matches the given
	 * glob string.  A refdb implementation may provide this function;
	 * if it is not provided, foreach will be used and the results filtered
	 * against the glob.
	 */
	int (*foreach_glob)(
		struct git_refdb_backend *backend,
		const char *glob,
		unsigned int list_flags,
		git_reference_foreach_cb callback,
		void *payload);

	/**
	 * Writes the given reference to the refdb.  A refdb implementation
	 * must provide this function.
	 */
	int (*write)(struct git_refdb_backend *backend, const git_reference *ref);

	/**
	 * Deletes the given reference from the refdb.  A refdb implementation
	 * must provide this function.
	 */
	int (*delete)(struct git_refdb_backend *backend, const git_reference *ref);

	/**
	 * Suggests that the given refdb compress or optimize its references.
	 * This mechanism is implementation specific.  (For on-disk reference
	 * databases, this may pack all loose references.)    A refdb
	 * implementation may provide this function; if it is not provided,
	 * nothing will be done.
	 */
	int (*compress)(struct git_refdb_backend *backend);

	/**
	 * Frees any resources held by the refdb.  A refdb implementation may
	 * provide this function; if it is not provided, nothing will be done.
	 */
	void (*free)(struct git_refdb_backend *backend);
};

#define GIT_ODB_BACKEND_VERSION 1
#define GIT_ODB_BACKEND_INIT {GIT_ODB_BACKEND_VERSION}

/**
 * Constructors for default refdb backends.
 */
GIT_EXTERN(int) git_refdb_backend_fs(
	struct git_refdb_backend **backend_out,
	git_repository *repo,
	git_refdb *refdb);

GIT_END_DECL

#endif
