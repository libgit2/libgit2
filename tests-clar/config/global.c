#include "clar_libgit2.h"
#include "buffer.h"
#include "fileops.h"

void test_config_global__initialize(void)
{
	git_buf path = GIT_BUF_INIT;

	cl_must_pass(p_mkdir("home", 0777));
	cl_git_pass(git_path_prettify(&path, "home", NULL));
	cl_git_pass(git_libgit2_opts(
		GIT_OPT_SET_SEARCH_PATH, GIT_CONFIG_LEVEL_GLOBAL, path.ptr));

	cl_must_pass(p_mkdir("xdg", 0777));
	cl_git_pass(git_path_prettify(&path, "xdg", NULL));
	cl_git_pass(git_libgit2_opts(
		GIT_OPT_SET_SEARCH_PATH, GIT_CONFIG_LEVEL_SYSTEM, path.ptr));

	cl_git_pass(git_libgit2_opts(
		GIT_OPT_SET_SEARCH_PATH, GIT_CONFIG_LEVEL_XDG, NULL));

	git_buf_free(&path);
}

void test_config_global__cleanup(void)
{
	cl_git_pass(git_futils_rmdir_r("home", NULL, GIT_RMDIR_REMOVE_FILES));
	cl_git_pass(git_futils_rmdir_r("xdg", NULL, GIT_RMDIR_REMOVE_FILES));
}

void test_config_global__open_global(void)
{
	git_config *cfg, *global, *selected, *dummy;

	cl_git_pass(git_config_open_default(&cfg));
	cl_git_pass(git_config_open_level(&global, cfg, GIT_CONFIG_LEVEL_GLOBAL));
	cl_git_fail(git_config_open_level(&dummy, cfg, GIT_CONFIG_LEVEL_XDG));
	cl_git_pass(git_config_open_global(&selected, cfg));

	git_config_free(selected);
	git_config_free(global);
	git_config_free(cfg);
}

void test_config_global__open_xdg(void)
{
	git_config *cfg, *xdg, *selected;
	const char *val, *str = "teststring";
	const char *key = "this.variable";

	p_setenv("XDG_CONFIG_HOME", "xdg", 1);

	cl_must_pass(p_mkdir("xdg/git/", 0777));
	cl_git_mkfile("xdg/git/config", "");

	cl_git_pass(git_config_open_default(&cfg));
	cl_git_pass(git_config_open_level(&xdg, cfg, GIT_CONFIG_LEVEL_XDG));
	cl_git_pass(git_config_open_global(&selected, cfg));

	cl_git_pass(git_config_set_string(xdg, key, str));
	cl_git_pass(git_config_get_string(&val, selected, key));
	cl_assert_equal_s(str, val);

	git_config_free(selected);
	git_config_free(xdg);
	git_config_free(cfg);
}
