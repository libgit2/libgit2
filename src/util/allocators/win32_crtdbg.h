/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#ifndef INCLUDE_util_allocators_crtdbg_h
#define INCLUDE_util_allocators_crtdbg_h

#include "git2_util.h"

#include "alloc.h"

int git_win32_crtdbg_init_allocator(git_allocator *allocator);

#endif
