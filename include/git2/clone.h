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
 * Clone a remote repository, and checkout the branch pointed to by the remote
 * HEAD.
 *
 * @param out pointer that will receive the resulting repository object
 * @param origin_remote a remote which will act as the initial fetch source
 * @param workdir_path local directory to clone to
 * @param fetch_progress_cb optional callback for fetch progress. Be aware that
 * this is called inline with network and indexing operations, so performance
 * may be affected.
 * @param fetch_progress_payload payload for fetch_progress_cb
 * @param checkout_opts options for the checkout step. If NULL, no checkout
 * is performed
 * @return 0 on success, GIT_ERROR otherwise (use giterr_last for information
 * about the error)
 */
GIT_EXTERN(int) git_clone(
		git_repository **out,
		git_remote *origin_remote,
		const char *workdir_path,
		git_checkout_opts *checkout_opts,
		git_transfer_progress_callback fetch_progress_cb,
		void *fetch_progress_payload);

/**
 * Create a bare clone of a remote repository.
 *
 * @param out pointer that will receive the resulting repository object
 * @param origin_remote a remote which will act as the initial fetch source
 * @param dest_path local directory to clone to
 * @param fetch_progress_cb optional callback for fetch progress. Be aware that
 * this is called inline with network and indexing operations, so performance
 * may be affected.
 * @param fetch_progress_payload payload for fetch_progress_cb
 * @return 0 on success, GIT_ERROR otherwise (use giterr_last for information about the error)
 */
GIT_EXTERN(int) git_clone_bare(
		git_repository **out,
		git_remote *origin_remote,
		const char *dest_path,
		git_transfer_progress_callback fetch_progress_cb,
		void *fetch_progress_payload);

/** @} */
GIT_END_DECL
#endif
