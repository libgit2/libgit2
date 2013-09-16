/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_utfconv_h__
#define INCLUDE_git_utfconv_h__

#include <wchar.h>
#include "common.h"

/* Maximum characters in a Windows path plus one for NUL byte */
#define GIT_WIN_PATH_UTF16 (260 + 1)

/* Maximum bytes necessary to convert a full-length UTF16 path to UTF8 */
#define GIT_WIN_PATH_UTF8  (260 * 4 + 1)

typedef wchar_t git_win32_path[GIT_WIN_PATH_UTF16];

typedef char git_win32_path_as_utf8[GIT_WIN_PATH_UTF8];

/* dest_size is the size of dest in wchar_t's */
int git__utf8_to_16(wchar_t * dest, size_t dest_size, const char *src);
/* dest_size is the size of dest in char's */
int git__utf16_to_8(char *dest, size_t dest_size, const wchar_t *src);

GIT_INLINE(int) git_win32_path_from_c(git_win32_path dest, const char *src)
{
	return git__utf8_to_16(dest, GIT_WIN_PATH_UTF16, src);
}

GIT_INLINE(int) git_win32_path_to_c(git_win32_path_as_utf8 dest, const wchar_t *src)
{
	return git__utf16_to_8(dest, GIT_WIN_PATH_UTF8, src);
}

#endif
