#include "clar_libgit2.h"
#include "posix.h"
#include "path.h"
#include "fileops.h"

static git_repository *g_repo = NULL;

void test_attr_ignore__initialize(void)
{
	g_repo = cl_git_sandbox_init("attr");
}

void test_attr_ignore__cleanup(void)
{
	cl_git_sandbox_cleanup();
	g_repo = NULL;
}

void assert_is_ignored(bool expected, const char *filepath)
{
	int is_ignored;

	cl_git_pass(git_ignore_path_is_ignored(&is_ignored, g_repo, filepath));
	cl_assert_equal_b(expected, is_ignored);
}

void test_attr_ignore__honor_temporary_rules(void)
{
	cl_git_rewritefile("attr/.gitignore", "/NewFolder\n/NewFolder/NewFolder");

	assert_is_ignored(false, "File.txt");
	assert_is_ignored(true, "NewFolder");
	assert_is_ignored(true, "NewFolder/NewFolder");
	assert_is_ignored(true, "NewFolder/NewFolder/File.txt");
}

void test_attr_ignore__allow_root(void)
{
	cl_git_rewritefile("attr/.gitignore", "/");

	assert_is_ignored(false, "File.txt");
	assert_is_ignored(false, "NewFolder");
	assert_is_ignored(false, "NewFolder/NewFolder");
	assert_is_ignored(false, "NewFolder/NewFolder/File.txt");
}

void test_attr_ignore__ignore_root(void)
{
	cl_git_rewritefile("attr/.gitignore", "/\n\n/NewFolder\n/NewFolder/NewFolder");

	assert_is_ignored(false, "File.txt");
	assert_is_ignored(true, "NewFolder");
	assert_is_ignored(true, "NewFolder/NewFolder");
	assert_is_ignored(true, "NewFolder/NewFolder/File.txt");
}


void test_attr_ignore__skip_gitignore_directory(void)
{
	cl_git_rewritefile("attr/.git/info/exclude", "/NewFolder\n/NewFolder/NewFolder");
	p_unlink("attr/.gitignore");
	cl_assert(!git_path_exists("attr/.gitignore"));
	p_mkdir("attr/.gitignore", 0777);
	cl_git_mkfile("attr/.gitignore/garbage.txt", "new_file\n");

	assert_is_ignored(false, "File.txt");
	assert_is_ignored(true, "NewFolder");
	assert_is_ignored(true, "NewFolder/NewFolder");
	assert_is_ignored(true, "NewFolder/NewFolder/File.txt");
}

void test_attr_ignore__expand_tilde_to_homedir(void)
{
	git_buf path = GIT_BUF_INIT;
	git_config *cfg;

	assert_is_ignored(false, "example.global_with_tilde");

	/* construct fake home with fake global excludes */

	cl_must_pass(p_mkdir("home", 0777));
	cl_git_pass(git_path_prettify(&path, "home", NULL));
	cl_git_pass(git_libgit2_opts(
		GIT_OPT_SET_SEARCH_PATH, GIT_CONFIG_LEVEL_GLOBAL, path.ptr));

	cl_git_mkfile("home/globalexcludes", "# found me\n*.global_with_tilde\n");

	cl_git_pass(git_repository_config(&cfg, g_repo));
	cl_git_pass(git_config_set_string(cfg, "core.excludesfile", "~/globalexcludes"));
	git_config_free(cfg);

	git_attr_cache_flush(g_repo); /* must reset to pick up change */

	assert_is_ignored(true, "example.global_with_tilde");

	cl_git_pass(git_futils_rmdir_r("home", NULL, GIT_RMDIR_REMOVE_FILES));

	cl_git_pass(git_libgit2_opts(
		GIT_OPT_SET_SEARCH_PATH, GIT_CONFIG_LEVEL_GLOBAL, NULL));

	git_buf_free(&path);
}
