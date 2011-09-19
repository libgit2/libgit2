/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include <wchar.h>

#ifndef INCLUDE_git_utf8conv_h__
#define INCLUDE_git_utf8conv_h__

wchar_t* conv_utf8_to_utf16(const char* str);
char* conv_utf16_to_utf8(const wchar_t* str);

#endif

