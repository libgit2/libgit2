/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#ifndef INCLUDE_w32_util_h__
#define INCLUDE_w32_util_h__

#include "utf-conv.h"

/**
 * Creates a FindFirstFile(Ex) filter string from a UTF-8 path.
 * The filter string enumerates all items in the directory.
 *
 * @param dest The buffer to receive the filter string.
 * @param src The UTF-8 path of the directory to enumerate.
 * @return True if the filter string was created successfully; false otherwise
 */
bool git_win32__findfirstfile_filter(git_win32_path dest, const char *src);

/**
 * Ensures the given path (file or folder) has the +H (hidden) attribute set.
 *
 * @param path The path which should receive the +H bit.
 * @return 0 on success; -1 on failure
 */
int git_win32__sethidden(const char *path);

#endif
