/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "error.h"

char *git_win32_get_error_message(DWORD error_code)
{
	LPWSTR lpMsgBuf = NULL;

	if (!error_code)
		return NULL;

	if (FormatMessageW(
				FORMAT_MESSAGE_ALLOCATE_BUFFER |
				FORMAT_MESSAGE_FROM_SYSTEM |
				FORMAT_MESSAGE_IGNORE_INSERTS,
				NULL, error_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
				(LPWSTR)&lpMsgBuf, 0, NULL)) {
		int utf8_size = WideCharToMultiByte(CP_UTF8, 0, lpMsgBuf, -1, NULL, 0, NULL, NULL);

		char *lpMsgBuf_utf8 = git__malloc(utf8_size * sizeof(char));
		if (lpMsgBuf_utf8 == NULL) {
			LocalFree(lpMsgBuf);
			return NULL;
		}
		if (!WideCharToMultiByte(CP_UTF8, 0, lpMsgBuf, -1, lpMsgBuf_utf8, utf8_size, NULL, NULL)) {
			LocalFree(lpMsgBuf);
			git__free(lpMsgBuf_utf8);
			return NULL;
		}

		LocalFree(lpMsgBuf);
		return lpMsgBuf_utf8;
	}
	return NULL;
}
