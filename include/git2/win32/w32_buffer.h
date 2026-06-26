/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_win32_w32_buffer_h__
#define INCLUDE_win32_w32_buffer_h__

#include "git2/git2_util.h"
#include "git2/str.h"

GIT_BEGIN_DECL

/**
 * Convert a wide character string to UTF-8 and append the results to the
 * buffer.
 */
int git_str_put_w(git_str *buf, const wchar_t *string_w, size_t len_w);

/** @} */
GIT_END_DECL

#endif
