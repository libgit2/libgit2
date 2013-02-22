#include "clar_libgit2.h"

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
