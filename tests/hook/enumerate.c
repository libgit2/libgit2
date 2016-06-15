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
	GIT_UNUSED(hook_name);
	GIT_UNUSED(payload);
	printf("hook: %s", hook_name);
	return 0;
}

void test_hook_enumerate__enumerate_hooks(void)
{
	cl_git_pass(git_hook_enumerate(g_repo, test_git_hook_foreach_cb, NULL));
}