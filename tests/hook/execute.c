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

static int hook_exec_2(git_hook_env env, void *payload)
{
	int *hook_called = payload;

	cl_assert_equal_i(env.args.count, 0);
	cl_assert_equal_s(git_buf_cstr(env.io), "input-data");

	*hook_called = 1;

	git_buf_clear(env.io);
	git_buf_puts(env.io, "output-data");

	return 0;
}

void test_hook_execute__hook_called_with_io(void)
{
	int hook_called = 0;
	git_buf input_data;

	git_buf_init(&input_data, 0);
	git_buf_puts(&input_data, "input-data");

	cl_must_pass(git_hook_register_callback(g_repo, hook_exec_2, &hook_called));
	cl_must_pass(git_hook_execute_io(&input_data, g_repo, "post-merge", NULL));

	cl_assert_equal_i_(hook_called, 1, "hook wasn't called");
	cl_assert_equal_s(git_buf_cstr(&input_data), "output-data");

	git_buf_free(&input_data);
}