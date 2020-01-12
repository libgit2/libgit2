#define GIT_ASSERT_HARD 0

#include "clar_libgit2.h"

static const char *hello_world = "hello, world";

static int dummy_fn(const char *myarg)
{
	GIT_ASSERT_ARG(myarg);
	GIT_ASSERT_ARG(myarg != hello_world);
	return 0;
}

static int bad_math(void)
{
	GIT_ASSERT(1 + 1 == 3);
	return 42;
}

void test_core_assert__argument(void)
{
	cl_git_fail(dummy_fn(NULL));
	cl_assert(git_error_last());
	cl_assert_equal_i(GIT_ERROR_INVALID, git_error_last()->klass);
	cl_assert_equal_s("invalid argument: 'myarg'", git_error_last()->message);

	cl_git_fail(dummy_fn(hello_world));
	cl_assert(git_error_last());
	cl_assert_equal_i(GIT_ERROR_INVALID, git_error_last()->klass);
	cl_assert_equal_s("invalid argument: 'myarg != hello_world'", git_error_last()->message);

	cl_git_pass(dummy_fn("foo"));
}

void test_core_assert__internal(void)
{
	cl_git_fail(bad_math());
	cl_assert(git_error_last());
	cl_assert_equal_i(GIT_ERROR_INTERNAL, git_error_last()->klass);
	cl_assert_equal_s("unrecoverable internal error: '1 + 1 == 3'", git_error_last()->message);
}
