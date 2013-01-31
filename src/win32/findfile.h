/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#ifndef INCLUDE_git_findfile_h__
#define INCLUDE_git_findfile_h__

struct win32_path {
	wchar_t path[MAX_PATH];
	DWORD len;
};

int win32_expand_path(struct win32_path *s_root, const wchar_t *templ);

int win32_find_file(git_buf *path, const struct win32_path *root, const char *filename);
int win32_find_system_file_using_path(git_buf *path, const char *filename);
int win32_find_system_file_using_registry(git_buf *path, const char *filename);
int win32_find_msysgit_in_registry(struct win32_path *root, const HKEY hieve, const wchar_t *key);

#endif

