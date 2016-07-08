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

void test_hook_enumerate__foreach_hooks(void)
{
	git_buf hook_list;
	git_buf expected_list;
	git_buf_init(&hook_list, 0);
	git_buf_init(&expected_list, 0);

	git_buf_puts(&expected_list, "commit-msg");
	git_buf_puts(&expected_list, "post-merge");

	cl_git_pass(git_hook_foreach(g_repo, test_git_hook_foreach_cb, &hook_list));

	cl_assert_equal_s(git_buf_cstr(&hook_list), git_buf_cstr(&expected_list));
	git_buf_free(&hook_list);
	git_buf_free(&expected_list);
}

#define ALT_HOOK_DIR "../testhooks"

void test_hook_enumerate__foreach_hooks_config_override(void)
{
	git_config *cfg;
	git_buf alt_hook = GIT_BUF_INIT;
	git_buf hook_list = GIT_BUF_INIT;
	git_buf expected_list = GIT_BUF_INIT;

	/* Change the repository hooks  */
	cl_git_pass(git_repository_config(&cfg, g_repo));
	cl_git_pass(git_config_set_string(cfg, "core.hooksPath", ALT_HOOK_DIR));

	cl_git_pass(git_hook_dir(&alt_hook, g_repo));

	/* Setup an alternate hook directory */
	cl_must_pass(p_mkdir(git_buf_cstr(&alt_hook), 0777));

	git_buf_joinpath(&alt_hook, alt_hook.ptr, "commit-msg");

	cl_git_mkfile(git_buf_cstr(&alt_hook), NULL);
	cl_must_pass(p_chmod(git_buf_cstr(&alt_hook), 0776));

	/* Reset our hook dir for the next hook */
	git_buf_rtruncate_at_char(&alt_hook, '/');
	git_buf_joinpath(&alt_hook, alt_hook.ptr, "post-merge");
	cl_git_mkfile(git_buf_cstr(&alt_hook), NULL);
	cl_must_pass(p_chmod(git_buf_cstr(&alt_hook), 0776));
	git_buf_free(&alt_hook);

	/* Check that we get the correct hooks */
	git_buf_init(&hook_list, 0);
	git_buf_init(&expected_list, 0);

	git_buf_puts(&expected_list, "commit-msg");
	git_buf_puts(&expected_list, "post-merge");

	cl_git_pass(git_hook_foreach(g_repo, test_git_hook_foreach_cb, &hook_list));

	cl_assert_equal_s(git_buf_cstr(&hook_list), git_buf_cstr(&expected_list));
	git_buf_free(&hook_list);
	git_buf_free(&expected_list);
}

#undef ALT_HOOK_DIR
