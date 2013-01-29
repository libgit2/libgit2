#include "clar_libgit2.h"

void test_core_opts__readwrite(void)
{
	size_t old_val = 0;
	size_t new_val = 0;

	git_libgit2_opts(GIT_OPT_GET_MWINDOW_SIZE, &old_val);
	git_libgit2_opts(GIT_OPT_SET_MWINDOW_SIZE, (size_t)1234);
	git_libgit2_opts(GIT_OPT_GET_MWINDOW_SIZE, &new_val);

	cl_assert(new_val == 1234);

	git_libgit2_opts(GIT_OPT_SET_MWINDOW_SIZE, old_val);
	git_libgit2_opts(GIT_OPT_GET_MWINDOW_SIZE, &new_val);

	cl_assert(new_val == old_val);
}

void test_core_opts__cfg_path(void)
{
	const char *old_system_path = NULL;
	const char *old_xdg_path = NULL;
	const char *old_global_path = NULL;

	const char *system_path = NULL;
	const char *xdg_path = NULL;
	const char *global_path = NULL;

	git_libgit2_opts(GIT_OPT_GET_CONFIG_PATH,GIT_CONFIG_LEVEL_SYSTEM,&old_system_path);
	git_libgit2_opts(GIT_OPT_GET_CONFIG_PATH,GIT_CONFIG_LEVEL_XDG,&old_xdg_path);
	git_libgit2_opts(GIT_OPT_GET_CONFIG_PATH,GIT_CONFIG_LEVEL_GLOBAL,&old_global_path);

	git_libgit2_opts(GIT_OPT_SET_CONFIG_PATH,GIT_CONFIG_LEVEL_SYSTEM,"system");
	git_libgit2_opts(GIT_OPT_SET_CONFIG_PATH,GIT_CONFIG_LEVEL_XDG,"xdg");
	git_libgit2_opts(GIT_OPT_SET_CONFIG_PATH,GIT_CONFIG_LEVEL_GLOBAL,"global");

	git_libgit2_opts(GIT_OPT_GET_CONFIG_PATH,GIT_CONFIG_LEVEL_SYSTEM,&system_path);
	git_libgit2_opts(GIT_OPT_GET_CONFIG_PATH,GIT_CONFIG_LEVEL_XDG,&xdg_path);
	git_libgit2_opts(GIT_OPT_GET_CONFIG_PATH,GIT_CONFIG_LEVEL_GLOBAL,&global_path);

	git_libgit2_opts(GIT_OPT_SET_CONFIG_PATH,GIT_CONFIG_LEVEL_SYSTEM,old_system_path);
	git_libgit2_opts(GIT_OPT_SET_CONFIG_PATH,GIT_CONFIG_LEVEL_XDG,old_xdg_path);
	git_libgit2_opts(GIT_OPT_SET_CONFIG_PATH,GIT_CONFIG_LEVEL_GLOBAL,old_global_path);

	cl_assert_equal_s("system",system_path);
	cl_assert_equal_s("xdg",xdg_path);
	cl_assert_equal_s("global",global_path);
}
