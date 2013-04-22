/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_sys_git_odb_backend_h__
#define INCLUDE_sys_git_odb_backend_h__

#include "git2/common.h"
#include "git2/types.h"
#include "git2/oid.h"
#include "git2/odb.h"

/**
 * @file git2/sys/backend.h
 * @brief Git custom backend implementors functions
 * @defgroup git_backend Git custom backend APIs
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * An instance for a custom backend
 */
struct git_odb_backend {
	unsigned int version;
	git_odb *odb;

	/* read and read_prefix each return to libgit2 a buffer which
	 * will be freed later. The buffer should be allocated using
	 * the function git_odb_backend_malloc to ensure that it can
	 * be safely freed later. */
	int (* read)(
		void **, size_t *, git_otype *, git_odb_backend *, const git_oid *);

	/* To find a unique object given a prefix
	 * of its oid.
	 * The oid given must be so that the
	 * remaining (GIT_OID_HEXSZ - len)*4 bits
	 * are 0s.
	 */
	int (* read_prefix)(
		git_oid *, void **, size_t *, git_otype *,
		git_odb_backend *, const git_oid *, size_t);

	int (* read_header)(
		size_t *, git_otype *, git_odb_backend *, const git_oid *);

	/* The writer may assume that the object
	 * has already been hashed and is passed
	 * in the first parameter.
	 */
	int (* write)(
		git_oid *, git_odb_backend *, const void *, size_t, git_otype);

	int (* writestream)(
		git_odb_stream **, git_odb_backend *, size_t, git_otype);

	int (* readstream)(
		git_odb_stream **, git_odb_backend *, const git_oid *);

	int (* exists)(
		git_odb_backend *, const git_oid *);

	int (* refresh)(git_odb_backend *);

	int (* foreach)(
		git_odb_backend *, git_odb_foreach_cb cb, void *payload);

	int (* writepack)(
		git_odb_writepack **, git_odb_backend *,
		git_transfer_progress_callback progress_cb, void *progress_payload);

	void (* free)(git_odb_backend *);
};

#define GIT_ODB_BACKEND_VERSION 1
#define GIT_ODB_BACKEND_INIT {GIT_ODB_BACKEND_VERSION}

GIT_EXTERN(void *) git_odb_backend_malloc(git_odb_backend *backend, size_t len);

GIT_END_DECL

#endif
