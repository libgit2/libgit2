#include <clar_libgit2.h>

static git_repository *g_repo = NULL;

void test_sparse_list__initialize(void)
{
}

void test_sparse_list__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

void test_sparse_list__lists_all_patterns(void)
{
	git_strarray patterns = {0};
	size_t i = 0;

	char *default_pattern__strings[] = { "/*", "!/*/" };
	git_strarray default_patterns = {default_pattern__strings, ARRAY_SIZE(default_pattern__strings) };
	git_sparse_checkout_init_options scopts = GIT_SPARSE_CHECKOUT_INIT_OPTIONS_INIT;

	g_repo = cl_git_sandbox_init("sparse");
	cl_git_pass(git_sparse_checkout_init(g_repo, &scopts));

	cl_git_pass(git_sparse_checkout_list(&patterns, g_repo));
	for (i = 0; i < patterns.count; i++) {
		cl_assert_equal_s(patterns.strings[i], default_patterns.strings[i]);
	}
}
