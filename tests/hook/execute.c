#include "clar_libgit2.h"

static git_repository *g_repo = NULL;

void test_hook_execute__initialize(void)
{
	g_repo = cl_git_sandbox_init("testrepo");
}

void test_hook_execute__cleanup(void)
{
	cl_git_sandbox_cleanup();
	g_repo = NULL;
}

static int hook_exec_1(git_hook_env env, void *payload)
{
	int *hook_called = payload;

	cl_assert_equal_i(env.args.count, 1);
	cl_assert_equal_s(env.args.strings[0], "1");

	*hook_called = 1;

	return 0;
}

void test_hook_execute__hook_called(void)
{
	int hook_called = 0;
	cl_must_pass(git_hook_register_callback(g_repo, hook_exec_1, &hook_called));
	cl_must_pass(git_hook_execute(g_repo, "post-merge", "1", NULL));
	cl_assert_equal_i_(hook_called, 1, "hook wasn't called");
}