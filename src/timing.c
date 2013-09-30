/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "timing.h"

#ifdef GIT_WIN32

#define TICKS_PER_SECOND	1000

int git_stopwatch_start(git_stopwatch *s)
{
	/* Try first to use the high-performance counter */
	if (!QueryPerformanceFrequency(&s->freq) ||
		!QueryPerformanceCounter(&s->start)) {
		/* Fall back to GetTickCount */
		s->freq.QuadPart = 0;
		s->start.LowPart = GetTickCount();
	}

	s->running = 1;

	return 0;
}

int git_stopwatch_query(double *out_elapsed_seconds, git_stopwatch *s)
{
	LARGE_INTEGER end;
	double elapsed_seconds;

	if (!s->running) {
		giterr_set(GITERR_INVALID, "Stopwatch is not running");
		return -1;
	}

	if (s->freq.QuadPart) {
		if (!QueryPerformanceCounter(&end)) {
			giterr_set(GITERR_OS, "Unable to read performance counter");
			return -1;
		}

		elapsed_seconds = (end.QuadPart - s->start.QuadPart) / (double)s->freq.QuadPart;
	} else {
		DWORD end_ticks = GetTickCount();

		/* Unsigned subtraction handles tick count wraparound */
		elapsed_seconds = (end_ticks - s->start.LowPart) / (double)TICKS_PER_SECOND;
	}

	/* Just to be on the safe side */
	if (elapsed_seconds < 0)
		elapsed_seconds = 0;

	*out_elapsed_seconds = elapsed_seconds;

	return 0;
}

#elif __APPLE__

int git_stopwatch_start(git_stopwatch *s)
{
	(void)mach_timebase_info(&s->info);
	s->start = mach_absolute_time();

	s->running = 1;

	return 0;
}

int git_stopwatch_query(double *out_elapsed_seconds, git_stopwatch *s)
{
	uint64_t end;
	double elapsed_seconds;

	if (!s->running) {
		giterr_set(GITERR_INVALID, "Stopwatch is not running");
		return -1;
	}

	end = mach_absolute_time();
	elapsed_seconds = 0;

	if (end > start) {
		elapsed_seconds = (double)s->info.numer * (end - start);
		elapsed_seconds /= (double)s->info.denom * 1E9L;
	}

	*out_elapsed_seconds = elapsed_seconds;

	return 0;
}

#else

int git_stopwatch_start(git_stopwatch *s)
{
	s->clk_id = GIT_PREFERRED_CLOCK;

	/* On EINVAL for the preferred clock, fall back to CLOCK_REALTIME */
	if (clock_gettime(s->clk_id, &s->start) &&
		(CLOCK_REALTIME == GIT_PREFERRED_CLOCK || errno != EINVAL ||
		 clock_gettime(s->clk_id = CLOCK_REALTIME, &s->start))) {
		giterr_set(GITERR_OS, "Unable to read the clock");
		s->running = 0;
		return -1;
	}

	s->running = 1;

	return 0;
}

int git_stopwatch_query(double *out_elapsed_seconds, git_stopwatch *s)
{
	struct timespec end, elapsed;
	double elapsed_seconds;

	if (!s->running) {
		giterr_set(GITERR_INVALID, "Stopwatch is not running");
		return -1;
	}

	if (clock_gettime(s->clk_id, &end)) {
		giterr_set(GITERR_OS, "Unable to read the clock");
		return -1;
	}

	elapsed_seconds = 0;

	/* If end < start, then we consider the elapsed time to be zero */
	if (end.tv_sec > s->start.tv_sec ||
		(end.tv_sec == s->start.tv_sec && end.tv_nsec > s->start.tv_nsec)) {
		elapsed.tv_sec = end.tv_sec - s->start.tv_sec;
		elapsed.tv_nsec = end.tv_nsec - s->start.tv_nsec;

		if (elapsed.tv_nsec < 0) {
			elapsed.tv_sec--;
			elapsed.tv_nsec += 1E9L;
		}

		elapsed_seconds = (double)elapsed.tv_sec + elapsed.tv_nsec / (double)1E9;
	}

	*out_elapsed_seconds = elapsed_seconds;

	return 0;
}

#endif
