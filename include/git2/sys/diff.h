/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_sys_git_diff_h__
#define INCLUDE_sys_git_diff_h__

#include "git2/common.h"
#include "git2/types.h"
#include "git2/oid.h"

/**
 * @file git2/sys/diff.h
 * @brief Low-level Git diff utilities
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * Diff print callback that writes to a git_buf.
 *
 * This function is provided not for you to call it directly, but instead
 * so you can use it as a function pointer to the `git_diff_print` or
 * `git_patch_print` APIs.  When using those APIs, you specify a callback
 * to actually handle the diff and/or patch data.
 *
 * Use this callback to easily write that data to a `git_buf` buffer.  You
 * must pass a `git_buf *` value as the payload to the `git_diff_print`
 * and/or `git_patch_print` function.  The data will be appended to the
 * buffer (after any existing content).
 */
GIT_EXTERN(int) git_diff_print_callback__to_buf(
	const git_diff_delta *delta,
	const git_diff_hunk *hunk,
	const git_diff_line *line,
	void *payload); /*< payload must be a `git_buf *` */

/**
 * Diff print callback that writes to stdio FILE handle.
 *
 * This function is provided not for you to call it directly, but instead
 * so you can use it as a function pointer to the `git_diff_print` or
 * `git_patch_print` APIs.  When using those APIs, you specify a callback
 * to actually handle the diff and/or patch data.
 *
 * Use this callback to easily write that data to a stdio FILE handle.  You
 * must pass a `FILE *` value (such as `stdout` or `stderr` or the return
 * value from `fopen()`) as the payload to the `git_diff_print`
 * and/or `git_patch_print` function.  If you pass NULL, this will write
 * data to `stdout`.
 */
GIT_EXTERN(int) git_diff_print_callback__to_file_handle(
	const git_diff_delta *delta,
	const git_diff_hunk *hunk,
	const git_diff_line *line,
	void *payload); /*< payload must be a `FILE *` */

/** @} */
GIT_END_DECL
#endif
