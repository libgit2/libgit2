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

/* @} */
GIT_END_DECL
#endif
