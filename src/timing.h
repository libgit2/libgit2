/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_timing_h__
#define INCLUDE_timing_h__

#include "common.h"

#ifndef GIT_WIN32
# include <time.h>
#endif

#ifdef __APPLE__
# include <mach/mach_time.h>
#endif

#ifdef GIT_WIN32

typedef struct git_stopwatch {
	LARGE_INTEGER freq;
	LARGE_INTEGER start;
	unsigned running : 1;
} git_stopwatch;

#elif __APPLE__

typedef struct git_stopwatch {
	mach_timebase_info_data_t info;
	uint64_t start;
	unsigned running : 1;
} git_stopwatch;

#else

typedef struct git_stopwatch {
	struct timespec start;
	unsigned running : 1;
} git_stopwatch;

#endif

extern int git_stopwatch_start(git_stopwatch *s);
extern int git_stopwatch_query(double *out_elapsed_seconds, git_stopwatch *s);

#endif
