#define CLAR_COUNTER_TV_DIFF(out_sec, out_usec, start_sec, start_usec, end_sec, end_usec) \
	if (start_usec > end_usec) { \
		out_sec = (end_sec - 1) - start_sec; \
		out_usec = (end_usec + 1000000) - start_usec; \
	} else { \
		out_sec = end_sec - start_sec; \
		out_usec = end_usec - start_usec; \
	}

#ifdef _WIN32

struct clar_counter {
	LARGE_INTEGER value;
};

void clar_counter_now(struct clar_counter *out)
{
	QueryPerformanceCounter(&out->value);
}

double clar_counter_diff(
	struct clar_counter *start,
	struct clar_counter *end)
{
	LARGE_INTEGER freq;

	QueryPerformanceFrequency(&freq);
	return (double)(end->value.QuadPart - start->value.QuadPart)/(double)freq.QuadPart;
}

#elif __APPLE__

#include <mach/mach_time.h>
#include <sys/time.h>

static double clar_counter_scaling_factor = -1;

struct clar_counter {
	union {
		uint64_t absolute_time;
		struct timeval tv;
	} val;
};

static void clar_counter_now(struct clar_counter *out)
{
	if (clar_counter_scaling_factor == 0) {
		mach_timebase_info_data_t info;

		clar_counter_scaling_factor =
			mach_timebase_info(&info) == KERN_SUCCESS ?
			((double)info.numer / (double)info.denom) / 1.0E6 :
			-1;
	}

	/* mach_timebase_info failed; fall back to gettimeofday */
	if (clar_counter_scaling_factor < 0)
		gettimeofday(&out->val.tv, NULL);
	else
		out->val.absolute_time = mach_absolute_time();
}

static double clar_counter_diff(
	struct clar_counter *start,
	struct clar_counter *end)
{
	if (clar_counter_scaling_factor < 0) {
		time_t sec;
		suseconds_t usec;

		CLAR_COUNTER_TV_DIFF(sec, usec,
			start->val.tv.tv_sec, start->val.tv.tv_usec,
			end->val.tv.tv_sec, end->val.tv.tv_usec);

		return (double)sec + ((double)usec / 1000000.0);
	} else {
		return (double)(end->val.absolute_time - start->val.absolute_time) *
			clar_counter_scaling_factor;
	}
}

#elif defined(__amigaos4__)

#include <proto/timer.h>

struct clar_counter {
	struct TimeVal tv;
}

static void clar_counter_now(struct clar_counter *out)
{
	ITimer->GetUpTime(&out->tv);
}

static double clar_counter_diff(
	struct clar_counter *start,
	struct clar_counter *end)
{
	uint32_t sec, usec;

	CLAR_COUNTER_TV_DIFF(sec, usec,
		start->tv.Seconds, start->tv.Microseconds,
		end->tv.Seconds, end->tv.Microseconds);

	return (double)sec + ((double)usec / 1000000.0);
}

#else

#include <sys/time.h>

struct clar_counter {
	int type;
	union {
#ifdef CLOCK_MONOTONIC
		struct timespec tp;
#endif
		struct timeval tv;
	} val;
};

static void clar_counter_now(struct clar_counter *out)
{
#ifdef CLOCK_MONOTONIC
	if (clock_gettime(CLOCK_MONOTONIC, &out->val.tp) == 0) {
		out->type = 0;
		return;
	}
#endif

	/* Fall back to using gettimeofday */
	out->type = 1;
	gettimeofday(&out->val.tv, NULL);
}

static double clar_counter_diff(
	struct clar_counter *start,
	struct clar_counter *end)
{
	time_t sec;
	suseconds_t usec;

#ifdef CLOCK_MONOTONIC
	if (start->type == 0) {
		time_t sec;
		long nsec;

		if (start->val.tp.tv_sec > end->val.tp.tv_sec) {
			sec = (end->val.tp.tv_sec - 1) - start->val.tp.tv_sec;
			nsec = (end->val.tp.tv_nsec + 1000000000) - start->val.tp.tv_nsec;
		} else {
			sec = end->val.tp.tv_sec - start->val.tp.tv_sec;
			nsec = end->val.tp.tv_nsec - start->val.tp.tv_nsec;
		}

		return (double)sec + ((double)nsec / 1000000000.0);
	}
#endif

	CLAR_COUNTER_TV_DIFF(sec, usec,
		start->val.tv.tv_sec, start->val.tv.tv_usec,
		end->val.tv.tv_sec, end->val.tv.tv_usec);

	return (double)sec + ((double)usec / 1000000.0);
}

#endif
