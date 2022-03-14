#include "clar_libgit2.h"
#include "sparse.h"
#include "git2/sparse.h"
#include "util.h"

static git_repository *g_repo = NULL;

void test_sparse_set__initialize(void)
{
}

void test_sparse_set__cleanup(void)
{
    cl_git_sandbox_cleanup();
}

void test_sparse_set__enables_sparse_checkout(void)
{
    const char *path;

    git_config *config;
    int b;

    char *pattern_strings[] = { "/*" };
    git_strarray patterns = { pattern_strings, ARRAY_SIZE(pattern_strings) };

    path = "sparse/.git/info/sparse-checkout";
    g_repo = cl_git_sandbox_init("sparse");

    cl_git_pass(git_sparse_checkout_set(g_repo, &patterns));

    cl_git_pass(git_repository_config(&config, g_repo));
	cl_git_pass(git_config_get_bool(&b, config, "core.sparseCheckout"));
    cl_assert_(b, "sparse checkout should be enabled");
    cl_assert_equal_b(git_fs_path_exists(path), true);

    git_config_free(config);
}

void test_sparse_set__rewrites_sparse_checkout_file(void)
{
    const char *path;
    git_str after_content = GIT_STR_INIT;

    char *initial_pattern_strings[] = { "foo", "bar", "biz", "baz" };
    git_strarray initial_patterns = { initial_pattern_strings, ARRAY_SIZE(initial_pattern_strings) };

    char *after_pattern_strings[] = { "bar", "baz" };
    git_strarray after_patterns = { after_pattern_strings, ARRAY_SIZE(after_pattern_strings) };
    const char *expected_string = "bar\nbaz";

    path = "sparse/.git/info/sparse-checkout";

    g_repo = cl_git_sandbox_init("sparse");

    cl_git_pass(git_sparse_checkout_set(g_repo, &initial_patterns));

    cl_git_pass(git_sparse_checkout_set(g_repo, &after_patterns));
    cl_git_pass(git_futils_readbuffer(&after_content, path));

    cl_assert_equal_s_(git_str_cstr(&after_content), expected_string, "git_sparse_checkout_set should overwrite existing patterns in the sparse-checkout file");
}

void test_sparse_set__applies_sparsity(void)
{
	char* pattern_strings[] = { "/a/" };
	git_strarray patterns = { pattern_strings, ARRAY_SIZE(pattern_strings) };

	g_repo = cl_git_sandbox_init("sparse");

	cl_git_pass(git_sparse_checkout_set(g_repo, &patterns));

	cl_assert_equal_b(git_fs_path_exists("sparse/file1"), false);
	cl_assert_equal_b(git_fs_path_exists("sparse/a/file3"), true);
}
