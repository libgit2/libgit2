#include "clar_libgit2.h"
#include "fileops.h"
#include <ctype.h>

git_buf path = GIT_BUF_INIT;

void test_repo_config__initialize(void)
{
	cl_fixture_sandbox("empty_standard_repo");
	cl_git_pass(cl_rename("empty_standard_repo/.gitted", "empty_standard_repo/.git"));

	git_buf_clear(&path);

	cl_must_pass(p_mkdir("alternate", 0777));
	cl_git_pass(git_path_prettify(&path, "alternate", NULL));

}

void test_repo_config__cleanup(void)
{
	cl_git_pass(git_futils_rmdir_r(path.ptr, NULL, GIT_RMDIR_REMOVE_FILES));

	git_buf_free(&path);
	cl_fixture_cleanup("empty_standard_repo");
}

void test_repo_config__open_missing_global(void)
{
	git_repository *repo;
	git_config *config, *global;

	cl_git_pass(git_libgit2_opts(
		GIT_OPT_SET_SEARCH_PATH, GIT_CONFIG_LEVEL_GLOBAL, path.ptr));
	cl_git_pass(git_libgit2_opts(
		GIT_OPT_SET_SEARCH_PATH, GIT_CONFIG_LEVEL_SYSTEM, path.ptr));
	cl_git_pass(git_libgit2_opts(
		GIT_OPT_SET_SEARCH_PATH, GIT_CONFIG_LEVEL_XDG, path.ptr));

	cl_git_pass(git_repository_open(&repo, "empty_standard_repo"));
	cl_git_pass(git_repository_config(&config, repo));
	cl_git_pass(git_config_open_level(&global, config, GIT_CONFIG_LEVEL_GLOBAL));

	cl_git_pass(git_config_set_string(global, "test.set", "42"));

	git_config_free(global);
	git_config_free(config);
	git_repository_free(repo);
}

void test_repo_config__open_missing_global_with_separators(void)
{
	git_repository *repo;
	git_config *config, *global;

	cl_git_pass(git_buf_printf(&path, "%c%s", GIT_PATH_LIST_SEPARATOR, "dummy"));

	cl_git_pass(git_libgit2_opts(
		GIT_OPT_SET_SEARCH_PATH, GIT_CONFIG_LEVEL_GLOBAL, path.ptr));
	cl_git_pass(git_libgit2_opts(
		GIT_OPT_SET_SEARCH_PATH, GIT_CONFIG_LEVEL_SYSTEM, path.ptr));
	cl_git_pass(git_libgit2_opts(
		GIT_OPT_SET_SEARCH_PATH, GIT_CONFIG_LEVEL_XDG, path.ptr));

	git_buf_free(&path);

	cl_git_pass(git_repository_open(&repo, "empty_standard_repo"));
	cl_git_pass(git_repository_config(&config, repo));
	cl_git_pass(git_config_open_level(&global, config, GIT_CONFIG_LEVEL_GLOBAL));

	cl_git_pass(git_config_set_string(global, "test.set", "42"));

	git_config_free(global);
	git_config_free(config);
	git_repository_free(repo);
}
