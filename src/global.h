/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_global_h__
#define INCLUDE_global_h__

#include "mwindow.h"
#include "hash.h"

#if defined(GIT_THREADS) && defined(_MSC_VER)
# define GIT_MEMORY_BARRIER MemoryBarrier()
#elif defined(GIT_THREADS)
# define GIT_MEMORY_BARRIER __sync_synchronize()
#else
# define GIT_MEMORY_BARRIER /* noop */
#endif

typedef struct {
	git_error *last_error;
	git_error error_t;
} git_global_st;

git_global_st *git__global_state(void);

extern git_mutex git__mwindow_mutex;

#define GIT_GLOBAL (git__global_state())

#endif
