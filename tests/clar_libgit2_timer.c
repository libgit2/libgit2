#include "clar_libgit2.h"
#include "clar_libgit2_timer.h"
#include "buffer.h"

void cl_perf_timer__init(cl_perf_timer *t)
{
	memset(t, 0, sizeof(cl_perf_timer));
}

#if defined(GIT_WIN32)

void cl_perf_timer__start(cl_perf_timer *t)
{
	QueryPerformanceCounter(&t->time_started);
}

void cl_perf_timer__stop(cl_perf_timer *t)
{
	LARGE_INTEGER time_now;
	QueryPerformanceCounter(&time_now);

	t->last.QuadPart = (time_now.QuadPart - t->time_started.QuadPart);
	t->sum.QuadPart += (time_now.QuadPart - t->time_started.QuadPart);
}

double cl_perf_timer__last(const cl_perf_timer *t)
{
	LARGE_INTEGER freq;
	double fraction;

	QueryPerformanceFrequency(&freq);

	fraction = ((double)t->last.QuadPart) / ((double)freq.QuadPart);
	return fraction;
}

double cl_perf_timer__sum(const cl_perf_timer *t)
{
	LARGE_INTEGER freq;
	double fraction;

	QueryPerformanceFrequency(&freq);

	fraction = ((double)t->sum.QuadPart) / ((double)freq.QuadPart);
	return fraction;
}

#else

#include <sys/time.h>

static uint32_t now_in_ms(void)
{
	struct timeval now;
	gettimeofday(&now, NULL);
	return (uint32_t)((now.tv_sec * 1000) + (now.tv_usec / 1000));
}

void cl_perf_timer__start(cl_perf_timer *t)
{
	t->time_started = now_in_ms();
}

void cl_perf_timer__stop(cl_perf_timer *t)
{
	uint32_t now = now_in_ms();
	t->last = (now - t->time_started);
	t->sum += (now - t->time_started);
}

double cl_perf_timer__last(const cl_perf_timer *t)
{
	double fraction = ((double)t->last) / 1000;
	return fraction;
}

double cl_perf_timer__sum(const cl_perf_timer *t)
{
	double fraction = ((double)t->sum) / 1000;
	return fraction;
}

#endif
