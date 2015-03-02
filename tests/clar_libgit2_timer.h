#ifndef __CLAR_LIBGIT2_TIMER__
#define __CLAR_LIBGIT2_TIMER__

#if defined(GIT_WIN32)

struct cl_perf_timer
{
	/* cummulative running time across all start..stop intervals */
	LARGE_INTEGER sum;
	/* value of last start..stop interval */
	LARGE_INTEGER last;
	/* clock value at start */
	LARGE_INTEGER time_started;
};

#define CL_PERF_TIMER_INIT {0}

#else

struct cl_perf_timer
{
	uint32_t sum;
	uint32_t last;
	uint32_t time_started;
};

#define CL_PERF_TIMER_INIT {0}

#endif

typedef struct cl_perf_timer cl_perf_timer;

void cl_perf_timer__init(cl_perf_timer *t);
void cl_perf_timer__start(cl_perf_timer *t);
void cl_perf_timer__stop(cl_perf_timer *t);

/**
 * return value of last start..stop interval in seconds.
 */
double cl_perf_timer__last(const cl_perf_timer *t);

/**
 * return cummulative running time across all start..stop
 * intervals in seconds.
 */
double cl_perf_timer__sum(const cl_perf_timer *t);

#endif /* __CLAR_LIBGIT2_TIMER__ */
