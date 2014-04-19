/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "w32_util.h"

/**
 * Creates a FindFirstFile(Ex) filter string from a UTF-8 path.
 * The filter string enumerates all items in the directory.
 *
 * @param dest The buffer to receive the filter string.
 * @param src The UTF-8 path of the directory to enumerate.
 * @return True if the filter string was created successfully; false otherwise
 */
bool git_win32__findfirstfile_filter(git_win32_path dest, const char *src)
{
	static const wchar_t suffix[] = L"\\*";
	int len = git_win32_path_from_utf8(dest, src);

	/* Ensure the path was converted */
	if (len < 0)
		return false;

	/* Ensure that the path does not end with a trailing slash --
	 * because we're about to add one! */
	if (len > 0 &&
		(dest[len - 1] == L'/' || dest[len - 1] == L'\\')) {
		dest[len - 1] = L'\0';
		len--;
	}

	/* Ensure we have enough room to add the suffix */
	if ((size_t)len > GIT_WIN_PATH_UTF16 - ARRAY_SIZE(suffix))
		return false;

	wcscat(dest, suffix);
	return true;
}

/**
 * Ensures the given path (file or folder) has the +H (hidden) attribute set.
 *
 * @param path The path which should receive the +H bit.
 * @return 0 on success; -1 on failure
 */
int git_win32__sethidden(const char *path)
{
	git_win32_path buf;
	DWORD attrs;

	if (git_win32_path_from_utf8(buf, path) < 0)
		return -1;

	attrs = GetFileAttributesW(buf);

	/* Ensure the path exists */
	if (INVALID_FILE_ATTRIBUTES == attrs)
		return -1;

	/* If the item isn't already +H, add the bit */
	if (0 == (attrs & FILE_ATTRIBUTE_HIDDEN) &&
		!SetFileAttributesW(buf, attrs | FILE_ATTRIBUTE_HIDDEN))
		return -1;

	return 0;
}
