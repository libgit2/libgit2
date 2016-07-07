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

GIT_EXTERN(int) git_hook_dir(git_buf *out_dir, git_repository *repo);

typedef int (*git_hook_foreach_cb)(const char *hook_name, void *payload);

GIT_EXTERN(int) git_hook_foreach(
	git_repository *repo,
	git_hook_foreach_cb callback,
	void *payload);

typedef struct {
	char *path;
	git_strarray args;
	git_buf *io;
} git_hook_env;

typedef void (*git_hook_destructor_cb)(void *payload);

typedef int (*git_hook_execution_cb)(
	git_hook_env env,
	void *payload);

GIT_EXTERN(int) git_hook_register_callback(
	git_repository *repo,
	git_hook_execution_cb executor,
	git_hook_destructor_cb destructor,
	void *payload);

GIT_EXTERN(int) git_hook_execute(
	git_repository *repo,
	const char *hook_name,
	...);

GIT_EXTERN(int) git_hook_execute_io(
	git_buf *io,
	git_repository *repo,
	const char *hook_name,
	...);

/* @} */
GIT_END_DECL
#endif
