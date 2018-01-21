/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_atexit_h__
#define INCLUDE_git_atexit_h__

#include "common.h"

/**
 * Execute cleanup for in-process operations
 *
 * This will e.g. delete lockfiles we own.
 */
GIT_EXTERN(int) git_atexit(void);

#endif /* INCLUDE_git_atexit_h__ */
