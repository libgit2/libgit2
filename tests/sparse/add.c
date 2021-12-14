#include "clar_libgit2.h"
#include "sparse.h"
#include "git2/sparse.h"
#include "util.h"

static git_repository *g_repo = NULL;

void test_sparse_add__initialize(void)
{
}

void test_sparse_add__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

void test_sparse_add__appends_to_patterns(void)
{
	size_t i = 0;
	git_strarray found_patterns = { 0 };

	char *expected_pattern_strings[] = { "/*", "!/*/", "/a/" };
	git_strarray expected_patterns = { expected_pattern_strings, ARRAY_SIZE(expected_pattern_strings) };

	char *pattern_strings[] = { "/a/" };
	git_strarray patterns = { pattern_strings, ARRAY_SIZE(pattern_strings) };

	git_sparse_checkout_init_options opts = GIT_SPARSE_CHECKOUT_INIT_OPTIONS_INIT;

	g_repo = cl_git_sandbox_init("sparse");

	cl_git_pass(git_sparse_checkout_init(g_repo, &opts));
	cl_git_pass(git_sparse_checkout_add(g_repo, &patterns));

	cl_git_pass(git_sparse_checkout_list(&found_patterns, g_repo));
	for (i = 0; i < found_patterns.count; i++) {
		cl_assert_equal_s(found_patterns.strings[i], expected_patterns.strings[i]);
	}
}

void test_sparse_add__applies_sparsity(void)
{
	char *pattern_strings[] = { "/a/" };
	git_strarray patterns = { pattern_strings, ARRAY_SIZE(pattern_strings) };

	git_sparse_checkout_init_options opts = GIT_SPARSE_CHECKOUT_INIT_OPTIONS_INIT;

	g_repo = cl_git_sandbox_init("sparse");

	cl_git_pass(git_sparse_checkout_init(g_repo, &opts));
	cl_git_pass(git_sparse_checkout_add(g_repo, &patterns));

	cl_assert_equal_b(git_fs_path_exists("sparse/a/file3"), true);
}
