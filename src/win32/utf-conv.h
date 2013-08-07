/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include <wchar.h>

#ifndef INCLUDE_git_utfconv_h__
#define INCLUDE_git_utfconv_h__

#define GIT_WIN_PATH_UTF16 (260 + 1)
#define GIT_WIN_PATH_UTF8  (260 * 4 + 1)

typedef wchar_t git_win_str_utf16[GIT_WIN_PATH_UTF16];
typedef char git_win_str_utf8[GIT_WIN_PATH_UTF8];

int git__utf8_to_16(git_win_str_utf16 dest, const git_win_str_utf8 src);
int git__utf16_to_8(git_win_str_utf8 dest, const git_win_str_utf16 src);

#endif

