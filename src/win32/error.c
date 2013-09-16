/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "error.h"

#ifdef GIT_WINHTTP
# include <winhttp.h>
#endif

#ifndef WC_ERR_INVALID_CHARS
#define WC_ERR_INVALID_CHARS	0x80
#endif

char *git_win32_get_error_message(DWORD error_code)
{
	LPWSTR lpMsgBuf = NULL;
	HMODULE hModule = NULL;
	char *utf8_msg = NULL;
	int utf8_size;
	DWORD dwFlags =
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS;

	if (!error_code)
		return NULL;

#ifdef GIT_WINHTTP
	/* Errors raised by WinHTTP are not in the system resource table */
	if (error_code >= WINHTTP_ERROR_BASE &&
		error_code <= WINHTTP_ERROR_LAST)
		hModule = GetModuleHandleW(L"winhttp");
#endif

	GIT_UNUSED(hModule);

	if (hModule)
		dwFlags |= FORMAT_MESSAGE_FROM_HMODULE;
	else
		dwFlags |= FORMAT_MESSAGE_FROM_SYSTEM;

	if (FormatMessageW(dwFlags, hModule, error_code,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPWSTR)&lpMsgBuf, 0, NULL)) {

		/* Invalid code point check supported on Vista+ only */
		if (git_has_win32_version(6, 0, 0))
			dwFlags = WC_ERR_INVALID_CHARS;
		else
			dwFlags = 0;

		utf8_size = WideCharToMultiByte(CP_UTF8, dwFlags,
			lpMsgBuf, -1, NULL, 0, NULL, NULL);

		if (!utf8_size) {
			assert(0);
			goto on_error;
		}

		utf8_msg = git__malloc(utf8_size);

		if (!utf8_msg)
			goto on_error;

		if (!WideCharToMultiByte(CP_UTF8, dwFlags,
			lpMsgBuf, -1, utf8_msg, utf8_size, NULL, NULL)) {
			git__free(utf8_msg);
			goto on_error;
		}

on_error:
		LocalFree(lpMsgBuf);
	}

	return utf8_msg;
}
