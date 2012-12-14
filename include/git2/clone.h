/*
 * Copyright (C) 2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_clone_h__
#define INCLUDE_git_clone_h__

#include "common.h"
#include "types.h"
#include "indexer.h"
#include "checkout.h"


/**
 * @file git2/clone.h
 * @brief Git cloning routines
 * @defgroup git_clone Git cloning routines
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * Clone options structure
 *
 * Use zeros to indicate default settings.  It's easiest to use the
 * `GIT_CLONE_OPTIONS_INIT` macro:
 *
 *		git_clone_options opts = GIT_CLONE_OPTIONS_INIT;
 *
 * - `out` is a pointer that receives the resulting repository object
 * - `origin_remote` is a remote which will act as the initial fetch source
 * - `local_path` is local directory to clone into
 * - `bare` should be set to zero to create a standard repo, non-zero for
 *   a bare repo
 * - `fetch_progress_cb` is optional callback for fetch progress. Be aware that
 *   this is called inline with network and indexing operations, so performance
 *   may be affected.
 * - `fetch_progress_payload` is payload for fetch_progress_cb
 * - `checkout_opts` is options for the checkout step. If NULL, no checkout
 *   is performed
 */

typedef struct git_clone_options {
	unsigned int version;

	git_repository **out;
	git_remote *origin_remote;
	const char *local_path;
	int bare;
	git_transfer_progress_callback fetch_progress_cb;
	void *fetch_progress_payload;
	git_checkout_opts *checkout_opts;
} git_clone_options;

#define GIT_CLONE_OPTIONS_VERSION 1
#define GIT_CLONE_OPTIONS_INIT {GIT_CLONE_OPTIONS_VERSION}

/**
 * Clone a remote repository, and checkout the branch pointed to by the remote
 * HEAD.
 *
 * @param options configuration options for the clone
 * @return 0 on success, GIT_ERROR otherwise (use giterr_last for information
 * about the error)
 */
GIT_EXTERN(int) git_clone(git_clone_options *options);

/** @} */
GIT_END_DECL
#endif
