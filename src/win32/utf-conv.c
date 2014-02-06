/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "utf-conv.h"

int git__utf8_to_16(wchar_t * dest, size_t dest_size, const char *src)
{
	return MultiByteToWideChar(CP_UTF8, 0, src, -1, dest, (int)dest_size);
}

int git__utf16_to_8(char *dest, size_t dest_size, const wchar_t *src)
{
	return WideCharToMultiByte(CP_UTF8, 0, src, -1, dest, (int)dest_size, NULL, NULL);
}
