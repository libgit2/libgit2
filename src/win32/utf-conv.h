/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include <wchar.h>

#ifndef INCLUDE_git_utfconv_h__
#define INCLUDE_git_utfconv_h__

typedef struct {
	int length;
	wchar_t data[MAX_PATH];
} gitwin_utf16_path;

int gitwin_path_create(gitwin_utf16_path **utf16_path, const char *str, size_t len);
void gitwin_path_free(gitwin_utf16_path *utf16_path);

#define gitwin_path_ptr(winpath) (winpath)->data

int gitwin_append_utf16(wchar_t *buffer, const char *str, size_t len);
char* gitwin_from_utf16(const wchar_t* str);

#endif

