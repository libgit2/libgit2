#include "clar_libgit2.h"
#include "array.h"
#include "cancellation.h"

void test_core_cancellation__can_cancel(void)
{
	git_cancellation *c;

	cl_git_pass(git_cancellation_new(&c));

	cl_assert(!git_cancellation_requested(c));
	cl_git_pass(git_cancellation_request(c));
	cl_assert(git_cancellation_requested(c));

	git_cancellation_free(c);
}

static int cancel_second(git_cancellation *cancellation, void *payload)
{
	git_cancellation *c;

	GIT_UNUSED(cancellation);

	c = (git_cancellation *) payload;
	cl_git_pass(git_cancellation_request(c));

	return 0;
}

void test_core_cancellation__can_register(void)
{
	git_cancellation *c1, *c2;

	cl_git_pass(git_cancellation_new(&c1));
	cl_git_pass(git_cancellation_new(&c2));

	cl_git_pass(git_cancellation_register(c1, cancel_second, c2));

	cl_assert(!git_cancellation_requested(c1));
	cl_assert(!git_cancellation_requested(c2));

	cl_git_pass(git_cancellation_request(c1));

	cl_assert(git_cancellation_requested(c1));
	cl_assert(git_cancellation_requested(c2));

	git_cancellation_free(c1);
	git_cancellation_free(c2);
}

static int cancel_count(git_cancellation *cancellation, void *payload)
{
	int *i;
	GIT_UNUSED(cancellation);

	i = (int *) payload;
	*i += 1;

	return 0;
}

void test_core_cancellation__registration_fires_once(void)
{
	git_cancellation *c;
	int cancelled_times = 0;

	cl_git_pass(git_cancellation_new(&c));

	cl_git_pass(git_cancellation_register(c, cancel_count, &cancelled_times));

	cl_assert(!git_cancellation_requested(c));

	cl_git_pass(git_cancellation_request(c));
	cl_assert(git_cancellation_requested(c));

	cl_git_pass(git_cancellation_request(c));
	cl_assert(git_cancellation_requested(c));

	cl_assert_equal_i(1, cancelled_times);

	git_cancellation_free(c);
}

static int cancel_fail(git_cancellation *cancellation, void *payload)
{
	int *i;
	GIT_UNUSED(cancellation);

	i = (int *) payload;
	*i += 1;

	return GIT_EUSER;
}

void test_core_cancellation__trigger_failure(void)
{
	git_cancellation *c;
	int cancelled_times = 0;

	cl_git_pass(git_cancellation_new(&c));

	/* Register twice, but we fail on the first time, so we should only inc once */
	cl_git_pass(git_cancellation_register(c, cancel_fail, &cancelled_times));
	cl_git_pass(git_cancellation_register(c, cancel_fail, &cancelled_times));

	cl_assert(!git_cancellation_requested(c));

	cl_git_fail_with(GIT_EUSER, git_cancellation_request(c));
	cl_assert(git_cancellation_requested(c));

	cl_git_pass(git_cancellation_request(c));
	cl_assert(git_cancellation_requested(c));

	cl_assert_equal_i(1, cancelled_times);

	git_cancellation_free(c);
}

void test_core_cancellation__detect_current(void)
{
	git_cancellation *c;

	cl_git_pass(git_cancellation_new(&c));
	cl_git_pass(git_cancellation_activate(c));

	cl_assert_equal_i(0, git_cancellation__canceled());
	cl_git_pass(git_cancellation_request(c));
	cl_assert_equal_i(1, git_cancellation__canceled());

	cl_git_pass(git_cancellation_deactivate());
}

