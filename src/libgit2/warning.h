/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_warning_h__
#define INCLUDE_warning_h__

#include "common.h"
#include "git2/warning.h"

extern int GIT_CALLBACK(git_warning__callback)(git_warning_t, void *, ...);
extern void *git_warning__data;

#define git_warning(warning, ...) \
	((git_warning__callback == NULL) ? -1 : git_warning__callback(warning, git_warning__data, __VA_ARGS__))

#endif
