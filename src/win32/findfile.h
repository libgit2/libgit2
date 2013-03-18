/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#ifndef INCLUDE_git_findfile_h__
#define INCLUDE_git_findfile_h__

struct git_win32__path {
	wchar_t path[MAX_PATH];
	DWORD len;
};

extern int git_win32__expand_path(
	struct git_win32__path *s_root, const wchar_t *templ);

extern int git_win32__find_file(
	git_buf *path, const struct git_win32__path *root, const char *filename);

extern int git_win32__find_system_dirs(git_buf *out);
extern int git_win32__find_global_dirs(git_buf *out);
extern int git_win32__find_xdg_dirs(git_buf *out);

#endif

