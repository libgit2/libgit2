/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_hook_h__
#define INCLUDE_git_hook_h__

#include "common.h"
#include "types.h"

/**
 * @file git2/hook.h
 * @brief Git hook management routines
 * @defgroup git_hook Git hook management routines
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * The callback used with git_hook_foreach.
 *
 * @see git_hook_foreach.
 *
 * @param hook_name The hook name.
 * @param payload A user-specified pointer.
 * @return 0 to continue looping, non-zero to stop.
 */
typedef int GIT_CALLBACK(git_hook_foreach_cb)(const char *hook_name, void *payload);

/**
 * Enumerate a repository's hooks.
 *
 * The callback will be called with the name of each available hook.
 *
 * @param repo The repository.
 * @param callback The enumeration callback.
 * @param payload A user-provided pointer that will be passed to the callback.
 * @return 0 on success, or an error code.
 */
GIT_EXTERN(int) git_hook_foreach(
	git_repository *repo,
	git_hook_foreach_cb callback,
	void *payload);

/**
 * A hook environment.
 *
 * This structure will be provided by the library when a hook needs to
 * be executed. You do not need to free it.
 */
typedef struct {
	/** The absolute path to the hook executable. */
	char *path;

	/** The argument list for the hook. */
	git_strarray args;

	/**
	 * On entering the hook executor, it will contain data that must be provided
	 * to the hook (i.e its stdin)
	 * On exiting the hook, you can set it to the hook output for
	 * FIXME: what for actually ?
	 */
	git_buf *io;
} git_hook_env;

/**
 * The destructor for a registered execution callback.
 *
 * @see git_hook_register_callback
 */
typedef void GIT_CALLBACK(git_hook_destructor_cb)(void *payload);

/**
 * The hook execution callback.
 *
 * @see git_hook_register_callback
 */
typedef int GIT_CALLBACK(git_hook_execution_cb)(git_hook_env *env, void *payload);

/**
 * Register an execution callback for the repository.
 *
 * As executing scripts is out of the scope of `libgit2`, this allows clients to
 * register a callback that will be called when a hook would be normally
 * executed. A `git_hook_env` structure describing what is expected of the
 * client will be provided.
 *
 * Note that this is intentionally *not meant* to replace `libgit2`'s callbacks.
 * This is just for compatibility with core Git, so that hooks can keep working.
 * As such, only one can be used at the same time.
 *
 * @param repo The repository
 * @param executor The hook execution callback.
 * @param destructor A payload cleanup callback.
 * @param payload A user-provided pointer that will be passed to the callback.
 * @return 0 on success, an error code otherwise.
 */
GIT_EXTERN(int) git_hook_register_callback(
	git_repository *repo,
	git_hook_execution_cb executor,
	git_hook_destructor_cb destructor,
	void *payload);

/**
 * Call the pre-commit hook, if available.
 */
GIT_EXTERN(int) git_hook_call_pre_commit(git_repository *repo);

/* modes for git_hook_call_prepare_commit_message */
#define GIT_HOOK_PREPARE_COMMIT_MSG_MESSAGE "message"
#define GIT_HOOK_PREPARE_COMMIT_MSG_TEMPLATE "template"
#define GIT_HOOK_PREPARE_COMMIT_MSG_MERGE "merge"
#define GIT_HOOK_PREPARE_COMMIT_MSG_SQUASH "squash"
#define GIT_HOOK_PREPARE_COMMIT_MSG_COMMIT "commit"

/**
 * Call the prepare-commit-msg hook, with a plain text message.
 */
GIT_EXTERN(int) git_hook_call_prepare_commit_message(git_repository *repo, const char *mode, ...);

/**
 * Call the commit-msg hook, with the given commit message.
 */
GIT_EXTERN(int) git_hook_call_commit_msg(git_repository *repo, const char *message);

/**
 * Call the post-commit hook.
 */
GIT_EXTERN(int) git_hook_call_post_commit(git_repository *repo);

/**
 * Call the pre-rebase hook.
 */
GIT_EXTERN(int) git_hook_call_pre_rebase(
	git_repository *repo,
	const git_annotated_commit *upstream,
	const git_annotated_commit *rebased);

/* @} */
GIT_END_DECL
#endif
