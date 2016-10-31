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
 * A helper to get the path of the repository's hooks.
 *
 * This will obey core.hooksPath.
 *
 * @param out_dir A pointer to a git_buf that will be filled with the absolute path.
 * @param repo The repository to get the hooks location from.
 * @return 0 on success, or an error code.
 */
GIT_EXTERN(int) git_hook_dir(git_buf *out_dir, git_repository *repo);

typedef int (*git_hook_foreach_cb)(const char *hook_name, void *payload);

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
 * A hook environment
 *
 * This structure will be provided by the library when a hook needs to
 * be executed.
 *
 * - `path` is the absolute path to the hook executable.
 * - `args` is the argument list for the hook.
 * - `io` is either an in/out pointer to either data that must be provided to
 *        the hook, a pointer where the executor will stash data outputted by
 *        the hook, or both.
 */
typedef struct {
	char *path;
	git_strarray args;
	git_buf *io;
} git_hook_env;

typedef void (*git_hook_destructor_cb)(void *payload);

typedef int (*git_hook_execution_cb)(
	git_hook_env *env,
	void *payload);

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
 * Trigger the execution of the named hook.
 *
 * @see `git_hook_execute_io`
 */
GIT_EXTERN(int) git_hook_execute(
	git_repository *repo,
	const char *hook_name,
	...);

/**
 * Trigger the execution of the named hook.
 *
 * @param io An in/out pointer to a git_buf. Will be passed as-is to the executor.
 * @param repo The repository.
 * @param hook_name The name of the hook.
 * @return 0 on success, an error code otherwise.
 */
GIT_EXTERN(int) git_hook_execute_io(
	git_buf *io,
	git_repository *repo,
	const char *hook_name,
	...);

/* @} */
GIT_END_DECL
#endif
