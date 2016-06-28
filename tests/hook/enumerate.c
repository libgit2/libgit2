#include "clar_libgit2.h"

static git_repository *g_repo = NULL;

void test_hook_enumerate__initialize(void)
{
	g_repo = cl_git_sandbox_init("testrepo");
}

void test_hook_enumerate__cleanup(void)
{
	cl_git_sandbox_cleanup();
	g_repo = NULL;
}

int test_git_hook_foreach_cb(const char *hook_name, void *payload)
{
	git_buf *hook_list = payload;
	git_buf_puts(hook_list, hook_name);
	return 0;
}

void test_hook_enumerate__enumerate_hooks(void)
{
	git_buf hook_list;
	git_buf expected_list;
	git_buf_init(&hook_list, 0);
	git_buf_init(&expected_list, 0);

	git_buf_puts(&expected_list, "commit-msg");
	git_buf_puts(&expected_list, "post-merge");

	cl_git_pass(git_hook_enumerate(g_repo, test_git_hook_foreach_cb, &hook_list));

	cl_assert_equal_s(git_buf_cstr(&hook_list), git_buf_cstr(&expected_list));
}