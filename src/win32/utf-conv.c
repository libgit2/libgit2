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

int git_win32_path_trim_end(wchar_t *str, size_t len)
{
	while (len > 0 && str[len-1] == L'\\')
		len--;

	str[len] = L'\0';

	return len;
}

int git_win32_path_unparse(git_win32_path str, int len)
{
	wchar_t *start = str;

	len = git_win32_path_trim_end(str, len);

	if (len < 4)
		return len;

	/* Strip leading \??\ */
	if (start[0] == L'\\' && start[1] == L'?' &&
		start[2] == L'?' && start[3] == L'\\') {
		start += 4;
		len -= 4;
	}

	/* Strip leading \\?\ */
	else if (start[0] == L'\\' && start[1] == L'\\' &&
		start[2] == L'?' && start[3] == L'\\') {
		start += 4;
		len -= 4;

		/* Strip leading \\?\UNC\ */
		if (len >= 4 && start[0] == L'U' && start[1] == L'N' &&
			start[2] == L'C' && start[3] == L'\\') {
			start += 4;
			len -= 4;
		}
	}

	if (start != str)
		memmove(str, start, len * sizeof(WCHAR));

	str[len] = L'\0';
	return len;
}
