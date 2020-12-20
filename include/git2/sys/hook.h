/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_sys_git_hook_h__
#define INCLUDE_sys_git_hook_h__

#include "git2/common.h"
#include "git2/types.h"
#include "git2/oid.h"

/**
 * @file git2/sys/hook.h
 * @brief Low-level Git hook calls
 * @defgroup git_hook_call Low-level Git hook calls
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * A helper to get the path of the repository's hooks.
 *
 * This will obey core.hooksPath.
 *
 * @param out_dir The absolute path to the hooks location.
 * @param repo The repository to get the hooks location from.
 * @return 0 on success, or an error code.
 */
GIT_EXTERN(int) git_hook_dir(git_buf *out_dir, git_repository *repo);

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
 * Caveats: you MUST pass a NULL sentinel to signal that there are no more
 * arguments.
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

/** @} */
GIT_END_DECL

#endif
