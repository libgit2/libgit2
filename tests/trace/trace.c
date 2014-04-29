#include "clar_libgit2.h"
#include "trace.h"

static int written = 0;

static void trace_callback(
	git_trace_level_t level,
	void *cb_payload,
	void *msg_payload,
	const char *msg)
{
	GIT_UNUSED(level); GIT_UNUSED(msg_payload);

	cl_assert(strcmp(msg, "Hello world!") == 0);

	if (cb_payload)
		*((int *)cb_payload) = 1;
}

void test_trace_trace__initialize(void)
{
	git_trace_set(GIT_TRACE_INFO_AND_BELOW, trace_callback, &written);
	written = 0;
}

void test_trace_trace__cleanup(void)
{
	git_trace_set(GIT_TRACE_NONE, NULL, NULL);
}

void test_trace_trace__sets(void)
{
#ifdef GIT_TRACE
	cl_assert(git_trace_level() == GIT_TRACE_INFO_AND_BELOW);
#endif
}

void test_trace_trace__can_reset(void)
{
#ifdef GIT_TRACE
	cl_assert(git_trace_level() == GIT_TRACE_INFO_AND_BELOW);
	cl_git_pass(git_trace_set(GIT_TRACE_ERROR, trace_callback, &written));

	cl_assert(written == 0);
	git_trace(GIT_TRACE_INFO, NULL, "Hello %s!", "world");
	cl_assert(written == 0);

	git_trace(GIT_TRACE_ERROR, NULL, "Hello %s!", "world");
	cl_assert(written == 1);
#endif
}

void test_trace_trace__can_unset(void)
{
#ifdef GIT_TRACE
	cl_assert(git_trace_level() == GIT_TRACE_INFO_AND_BELOW);
	cl_git_pass(git_trace_set(GIT_TRACE_NONE, NULL, NULL));

	cl_assert(git_trace_level() == GIT_TRACE_NONE);

	cl_assert(written == 0);
	git_trace(GIT_TRACE_FATAL, NULL, "Hello %s!", "world");
	cl_assert(written == 0);
#endif
}

void test_trace_trace__skips_higher_level(void)
{
#ifdef GIT_TRACE
	cl_assert(written == 0);
	git_trace(GIT_TRACE_DEBUG, NULL, "Hello %s!", "world");
	cl_assert(written == 0);
#endif
}

void test_trace_trace__writes(void)
{
#ifdef GIT_TRACE
	cl_assert(written == 0);
	git_trace(GIT_TRACE_INFO, NULL, "Hello %s!", "world");
	cl_assert(written == 1);
#endif
}

void test_trace_trace__writes_lower_level(void)
{
#ifdef GIT_TRACE
	cl_assert(written == 0);
	git_trace(GIT_TRACE_ERROR, NULL, "Hello %s!", "world");
	cl_assert(written == 1);
#endif
}
