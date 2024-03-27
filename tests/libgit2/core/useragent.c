#include "clar_libgit2.h"
#include "settings.h"

static git_buf default_ua = GIT_BUF_INIT;
static git_buf default_product = GIT_BUF_INIT;

void test_core_useragent__initialize(void)
{
	cl_git_pass(git_libgit2_opts(GIT_OPT_GET_USER_AGENT, &default_ua));
	cl_git_pass(git_libgit2_opts(GIT_OPT_GET_USER_AGENT_PRODUCT, &default_product));
}

void test_core_useragent__cleanup(void)
{
	git_libgit2_opts(GIT_OPT_SET_USER_AGENT, NULL);
	git_libgit2_opts(GIT_OPT_SET_USER_AGENT_PRODUCT, NULL);

	git_buf_dispose(&default_ua);
	git_buf_dispose(&default_product);
}

void test_core_useragent__get_default(void)
{
	cl_assert(default_ua.size);
	cl_assert(default_ua.ptr);
	cl_assert(git__prefixcmp(default_ua.ptr, "libgit2 ") == 0);

	cl_assert(default_product.size);
	cl_assert(default_product.ptr);
	cl_assert(git__prefixcmp(default_product.ptr, "git/") == 0);
}

void test_core_useragent__set(void)
{
	cl_git_pass(git_libgit2_opts(GIT_OPT_SET_USER_AGENT, "foo bar 4.24"));
	cl_assert_equal_s("foo bar 4.24", git_settings__user_agent());
	cl_assert_equal_s(default_product.ptr, git_settings__user_agent_product());

	cl_git_pass(git_libgit2_opts(GIT_OPT_SET_USER_AGENT_PRODUCT, "baz/2.2.3"));
	cl_assert_equal_s("foo bar 4.24", git_settings__user_agent());
	cl_assert_equal_s("baz/2.2.3", git_settings__user_agent_product());

	cl_git_pass(git_libgit2_opts(GIT_OPT_SET_USER_AGENT, ""));
	cl_git_pass(git_libgit2_opts(GIT_OPT_SET_USER_AGENT_PRODUCT, ""));
	cl_assert_equal_s("", git_settings__user_agent());
	cl_assert_equal_s("", git_settings__user_agent_product());

	cl_git_pass(git_libgit2_opts(GIT_OPT_SET_USER_AGENT, NULL));
	cl_git_pass(git_libgit2_opts(GIT_OPT_SET_USER_AGENT_PRODUCT, NULL));
	cl_assert_equal_s(default_ua.ptr, git_settings__user_agent());
	cl_assert_equal_s(default_product.ptr, git_settings__user_agent_product());
}
