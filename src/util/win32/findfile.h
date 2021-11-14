/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#ifndef INCLUDE_win32_findfile_h__
#define INCLUDE_win32_findfile_h__

#include "git2_util.h"

/** Sets the mock registry root for Git for Windows for testing. */
extern int git_win32__set_registry_system_dir(const wchar_t *mock_sysdir);

extern int git_win32__find_system_dirs(git_str *out, const char *subpath);
extern int git_win32__find_global_dirs(git_str *out);
extern int git_win32__find_xdg_dirs(git_str *out);
extern int git_win32__find_programdata_dirs(git_str *out);

#endif

