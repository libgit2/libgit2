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

typedef int (*git_hook_foreach_cb)(const char *hook_name, void *payload);

GIT_EXTERN(int) git_hook_enumerate(
	git_repository *repo,
	git_hook_foreach_cb callback,
	void *payload
);



typedef int (*git_hook_execute_cb)(
	const git_buf *hook_path,
	void *payload,
	...
);

GIT_EXTERN(int) git_hook_register(
	git_repository *repo,
	const char *hook_name,
	git_hook_execute_cb callback,
	void *payload
);

/* @} */
GIT_END_DECL
#endif