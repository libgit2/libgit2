#include "clar_libgit2.h"

#include "date.h"

void test_date_date__overflow(void)
{
#ifdef __LP64__
   git_time_t d2038, d2039;

   /* This is expected to fail on a 32-bit machine. */
   cl_git_pass(git_date_parse(&d2038, "2038-1-1"));
   cl_git_pass(git_date_parse(&d2039, "2039-1-1"));
   cl_assert(d2038 < d2039);
#endif
}

void test_date_date__invalid_date(void)
{
   git_time_t d;
   cl_git_fail(git_date_parse(&d, ""));
   cl_git_fail(git_date_parse(&d, "NEITHER_INTEGER_NOR_DATETIME"));
}

void test_date_date__offset(void)
{
	git_time_t d;
	int offset;
	cl_git_pass(git_date_offset_parse(&d, &offset, "1970-1-1 01:00:00+03"));
	cl_assert_equal_i(offset, 3*60);
}

void test_date_date__relative_overflow(void)
{
#ifdef __LP64__
	git_time_t d, now = time(NULL);

	/* 5000 * 7 days in seconds doesn't fit in an int; it came out in the future */
	cl_git_pass(git_date_parse(&d, "5000 fridays ago"));
	cl_assert(d < now - (git_time_t)4999 * 7 * 24 * 60 * 60);
#endif
}

void test_date_date__relative_number_too_large(void)
{
	git_time_t d, now = time(NULL);

	/* this was cut down to "1 day ago" */
	cl_git_pass(git_date_parse(&d, "4294967297 days ago"));
	cl_assert(d >= now);

	/* these were cut down to INT_MIN, which can't be subtracted */
	cl_git_pass(git_date_parse(&d, "6442450944 years ago"));
	cl_assert(d >= now);

	cl_git_pass(git_date_parse(&d, "6442450944 months ago"));
	cl_assert(d >= now);
}
