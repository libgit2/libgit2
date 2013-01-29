#include "clar_libgit2.h"
#include "config.h"

static const char *system_path;
static const char *xdg_path;
static const char *global_path;

void test_config_default__initialize(void)
{
	git_libgit2_opts(GIT_OPT_GET_CONFIG_PATH, GIT_CONFIG_LEVEL_SYSTEM, &system_path);
	git_libgit2_opts(GIT_OPT_GET_CONFIG_PATH, GIT_CONFIG_LEVEL_XDG, &xdg_path);
	git_libgit2_opts(GIT_OPT_GET_CONFIG_PATH, GIT_CONFIG_LEVEL_GLOBAL, &global_path);
}

void test_config_default__cleanup(void)
{
	git_libgit2_opts(GIT_OPT_SET_CONFIG_PATH, GIT_CONFIG_LEVEL_GLOBAL, global_path);
	git_libgit2_opts(GIT_OPT_SET_CONFIG_PATH, GIT_CONFIG_LEVEL_XDG, xdg_path);
	git_libgit2_opts(GIT_OPT_SET_CONFIG_PATH, GIT_CONFIG_LEVEL_SYSTEM, system_path);
}

void test_config_default__open_default_must_fail_if_no_cfg_can_be_found(void)
{
	git_config *cfg;

	git_libgit2_opts(GIT_OPT_SET_CONFIG_PATH, GIT_CONFIG_LEVEL_SYSTEM, "non-existent-config-file");
	git_libgit2_opts(GIT_OPT_SET_CONFIG_PATH, GIT_CONFIG_LEVEL_XDG, "non-existent-config-file");
	git_libgit2_opts(GIT_OPT_SET_CONFIG_PATH, GIT_CONFIG_LEVEL_GLOBAL, "non-existent-config-file");

	cl_git_fail(git_config_open_default(&cfg));
}
