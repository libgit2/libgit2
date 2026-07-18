/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#ifndef INCLUDE_runtime_h__
#define INCLUDE_runtime_h__

#include "git2/git2_util.h"

GIT_BEGIN_DECL

GIT_EXTERN(int) git_strlist_copy(char ***out, const char **in, size_t len);

GIT_EXTERN(int) git_strlist_copy_with_null(
	char ***out,
	const char **in,
	size_t len);

GIT_EXTERN(bool) git_strlist_contains_prefix(
	const char **strings,
	size_t len,
	const char *str,
	size_t n);

GIT_EXTERN(bool) git_strlist_contains_key(
	const char **strings,
	size_t len,
	const char *key,
	char delimiter);

GIT_EXTERN(void) git_strlist_free(char **strings, size_t len);

GIT_EXTERN(void) git_strlist_free_with_null(char **strings);

/** @} */
GIT_END_DECL

#endif
