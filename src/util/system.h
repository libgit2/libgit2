/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#ifndef INCLUDE_system_h__
#define INCLUDE_system_h__

#include "git2_util.h"

/**
 * Get the home directory for the current user.
 *
 * @param out the buffer to store the home directory in
 * @return 0 on success, or an error code
 */
extern int git_system_homedir(git_str *out);

#endif
