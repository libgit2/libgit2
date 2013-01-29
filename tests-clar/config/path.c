#include "clar_libgit2.h"
#include "fileops.h"
#include "config.h"

static const char *system_path;
static const char *xdg_path;
static const char *global_path;

void test_config_path__initialize(void)
{
	git_libgit2_opts(GIT_OPT_GET_CONFIG_PATH, GIT_CONFIG_LEVEL_SYSTEM, &system_path);
	git_libgit2_opts(GIT_OPT_GET_CONFIG_PATH, GIT_CONFIG_LEVEL_XDG, &xdg_path);
	git_libgit2_opts(GIT_OPT_GET_CONFIG_PATH, GIT_CONFIG_LEVEL_GLOBAL, &global_path);
}

void test_config_path__cleanup(void)
{
	git_libgit2_opts(GIT_OPT_SET_CONFIG_PATH, GIT_CONFIG_LEVEL_GLOBAL, global_path);
	git_libgit2_opts(GIT_OPT_SET_CONFIG_PATH, GIT_CONFIG_LEVEL_XDG, xdg_path);
	git_libgit2_opts(GIT_OPT_SET_CONFIG_PATH, GIT_CONFIG_LEVEL_SYSTEM, system_path);
}

void test_config_path__non_existent_user_supplied_config_file(void)
{
	git_buf path = GIT_BUF_INIT;

	git_libgit2_opts(GIT_OPT_SET_CONFIG_PATH, GIT_CONFIG_LEVEL_SYSTEM, "non-existent-config-file");
	git_libgit2_opts(GIT_OPT_SET_CONFIG_PATH, GIT_CONFIG_LEVEL_XDG, "non-existent-config-file");
	git_libgit2_opts(GIT_OPT_SET_CONFIG_PATH, GIT_CONFIG_LEVEL_GLOBAL, "non-existent-config-file");

	cl_git_fail(git_config_find_system_r(&path));
	cl_git_fail(git_config_find_xdg_r(&path));
	cl_git_fail(git_config_find_global_r(&path));
}

void test_config_path__user_supplied_config_file(void)
{
	const char *config0_path = cl_fixture("config/config0");
	const char *config1_path = cl_fixture("config/config1");
	const char *config2_path = cl_fixture("config/config2");
	git_buf path = GIT_BUF_INIT;

	git_libgit2_opts(GIT_OPT_SET_CONFIG_PATH, GIT_CONFIG_LEVEL_SYSTEM, config0_path);
	git_libgit2_opts(GIT_OPT_SET_CONFIG_PATH, GIT_CONFIG_LEVEL_XDG, config1_path);
	git_libgit2_opts(GIT_OPT_SET_CONFIG_PATH, GIT_CONFIG_LEVEL_GLOBAL, config2_path);

	cl_git_pass(git_config_find_system_r(&path));
	cl_assert_equal_s(config0_path,path.ptr);
	cl_git_pass(git_config_find_xdg_r(&path));
	cl_assert_equal_s(config1_path,path.ptr);
	cl_git_pass(git_config_find_global_r(&path));
	cl_assert_equal_s(config2_path,path.ptr);
}
