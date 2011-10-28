/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "utf-conv.h"

/*
 * Default codepage value
 */
static int _active_codepage = CP_UTF8;

void gitwin_set_codepage(unsigned int codepage)
{
	_active_codepage = codepage;
}

unsigned int gitwin_get_codepage(void)
{
	return _active_codepage;
}

void gitwin_set_utf8(void)
{
	_active_codepage = CP_UTF8;
}

wchar_t* gitwin_to_utf16(const char* str)
{
	wchar_t* ret;
	int cb;

	if (!str) {
		return NULL;
	}

	cb = strlen(str) * sizeof(wchar_t);
	if (cb == 0) {
		ret = (wchar_t*)git__malloc(sizeof(wchar_t));
		ret[0] = 0;
		return ret;
	}

	/* Add space for null terminator */
	cb += sizeof(wchar_t);

	ret = (wchar_t*)git__malloc(cb);

	if (MultiByteToWideChar(_active_codepage, 0, str, -1, ret, cb) == 0) {
		git__free(ret);
		ret = NULL;
	}

	return ret;
}

char* gitwin_from_utf16(const wchar_t* str)
{
	char* ret;
	int cb;

	if (!str) {
		return NULL;
	}

	cb = wcslen(str) * sizeof(char);
	if (cb == 0) {
		ret = (char*)git__malloc(sizeof(char));
		ret[0] = 0;
		return ret;
	}

	/* Add space for null terminator */
	cb += sizeof(char);

	ret = (char*)git__malloc(cb);

	if (WideCharToMultiByte(_active_codepage, 0, str, -1, ret, cb, NULL, NULL) == 0) {
		git__free(ret);
		ret = NULL;
	}

	return ret;

}
