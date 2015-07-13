/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "utf-conv.h"

GIT_INLINE(void) git__set_errno(void)
{
	if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
		errno = ENAMETOOLONG;
	else
		errno = EINVAL;
}

/**
 * Converts a UTF-8 string to wide characters.
 *
 * @param dest The buffer to receive the wide string.
 * @param dest_size The size of the buffer, in characters.
 * @param src The UTF-8 string to convert.
 * @return The length of the wide string, in characters (not counting the NULL terminator), or < 0 for failure
 */
int git__utf8_to_16(wchar_t *dest, size_t dest_size, const char *src)
{
	int len;

	size_t stCharConv = 0;
	if (len = mbstowcs_s(&stCharConv, dest, dest_size, src, _TRUNCATE) < 0)
		git__set_errno();

	len = stCharConv;
	return len;
}

/**
 * Converts a wide string to UTF-8.
 *
 * @param dest The buffer to receive the UTF-8 string.
 * @param dest_size The size of the buffer, in bytes.
 * @param src The wide string to convert.
 * @return The length of the UTF-8 string, in bytes (not counting the NULL terminator), or < 0 for failure
 */
int git__utf16_to_8(char *dest, size_t dest_size, const wchar_t *src)
{
	int len;

	size_t stCharConv = 0;
	if (len = wcstombs_s(&stCharConv, dest, dest_size, src, _TRUNCATE) < 0)
		git__set_errno();

	len = stCharConv;
	return len;
}

/**
 * Converts a UTF-8 string to wide characters.
 * Memory is allocated to hold the converted string.
 * The caller is responsible for freeing the string with git__free.
 *
 * @param dest Receives a pointer to the wide string.
 * @param src The UTF-8 string to convert.
 * @return The length of the wide string, in characters (not counting the NULL terminator), or < 0 for failure
 */
int git__utf8_to_16_alloc(wchar_t **dest, const char *src)
{
	size_t stCharConv = 0;
	int utf16_size;

	*dest = NULL;

	utf16_size = mbstowcs_s(&stCharConv, NULL, 0, src, 0);

	if (utf16_size < 0) {
		git__set_errno();
		return -1;
	}
	
	/* Set the size to the required buffer size */
	utf16_size = stCharConv;

	*dest = git__mallocarray(utf16_size, sizeof(wchar_t));

	if (!*dest) {
		errno = ENOMEM;
		return -1;
	}

	utf16_size = mbstowcs_s(&stCharConv, *dest, utf16_size, src, _TRUNCATE);

	if (utf16_size < 0) {
		git__set_errno();

		git__free(*dest);
		*dest = NULL;
		return utf16_size;
	}

	utf16_size = stCharConv;
	return utf16_size;
}

/**
 * Converts a wide string to UTF-8.
 * Memory is allocated to hold the converted string.
 * The caller is responsible for freeing the string with git__free.
 *
 * @param dest Receives a pointer to the UTF-8 string.
 * @param src The wide string to convert.
 * @return The length of the UTF-8 string, in bytes (not counting the NULL terminator), or < 0 for failure
 */
int git__utf16_to_8_alloc(char **dest, const wchar_t *src)
{
	size_t stCharConv = 0;
	int utf8_size;

	*dest = NULL;

	utf8_size = wcstombs_s(&stCharConv, NULL, 0, src, 0);

	if (utf8_size < 0) {
		git__set_errno();
		return -1;
	}
	
	/* Set the size to the required buffer size */
	utf8_size = stCharConv;

	*dest = git__malloc(utf8_size);

	if (!*dest) {
		errno = ENOMEM;
		return -1;
	}

	utf8_size = wcstombs_s(&stCharConv, *dest, utf8_size, src, _TRUNCATE);

	if (utf8_size < 0) {
		git__set_errno();

		git__free(*dest);
		*dest = NULL;
		return utf8_size;
	}

	utf8_size = stCharConv;
	return utf8_size;
}