#include "clar_libgit2.h"
#include "posix.h"
#include "path.h"

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
	cl_assert_equal_i(expected, is_ignored == 1);
}

void test_attr_ignore__honor_temporary_rules(void)
{
	cl_git_rewritefile("attr/.gitignore", "/NewFolder\n/NewFolder/NewFolder");

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
