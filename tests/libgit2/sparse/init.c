#include "clar_libgit2.h"
#include "sparse.h"
#include "git2/sparse.h"

static git_repository *g_repo = NULL;

void test_sparse_init__initialize(void)
{
}

void test_sparse_init__cleanup(void)
{
    cl_git_sandbox_cleanup();
}

void test_sparse_init__enables_sparse_checkout(void)
{
	git_config *config;
	int b;

	git_sparse_checkout_init_options opts = GIT_SPARSE_CHECKOUT_INIT_OPTIONS_INIT;

	g_repo = cl_git_sandbox_init("sparse");

	cl_git_pass(git_sparse_checkout_init(g_repo, &opts));

	cl_git_pass(git_repository_config(&config, g_repo));
	cl_git_pass(git_config_get_bool(&b, config, "core.sparseCheckout"));
	cl_assert_(b, "sparse checkout should be enabled");

	git_config_free(config);
}

void test_sparse_init__writes_sparse_checkout_file(void)
{
    const char *path;
	git_str content = GIT_STR_INIT;
    git_sparse_checkout_init_options opts = GIT_SPARSE_CHECKOUT_INIT_OPTIONS_INIT;

    path = "sparse/.git/info/sparse-checkout";
    g_repo = cl_git_sandbox_init("sparse");

    cl_git_pass(git_sparse_checkout_init(g_repo, &opts));
    cl_assert_equal_b(git_fs_path_exists(path), true);

	cl_git_pass(git_futils_readbuffer(&content, path));
	cl_assert_(strlen(git_str_cstr(&content)) > 1,"git_sparse_checkout_init should not init an empty file");
}

void test_sparse_init__sets_default_patterns(void)
{
	size_t i = 0;
	char *default_pattern_strings[] = { "/*", "!/*/" };
	git_strarray default_patterns = { default_pattern_strings, ARRAY_SIZE(default_pattern_strings) };
	git_strarray found_patterns = { 0 };

	git_sparse_checkout_init_options opts = GIT_SPARSE_CHECKOUT_INIT_OPTIONS_INIT;

	g_repo = cl_git_sandbox_init("sparse");

	cl_git_pass(git_sparse_checkout_init(g_repo, &opts));

	cl_git_pass(git_sparse_checkout_list(&found_patterns, g_repo));
	for (i = 0; i < found_patterns.count; i++) {
		cl_assert_equal_s(found_patterns.strings[i], default_patterns.strings[i]);
	}
}

void test_sparse_init__does_not_overwrite_existing_file(void)
{
	size_t i = 0;
	char *initial_pattern_strings[] = { "foo", "bar", "biz", "baz" };
	git_strarray initial_patterns = { initial_pattern_strings, ARRAY_SIZE(initial_pattern_strings) };
	git_strarray found_patterns = { 0 };

	git_sparse_checkout_init_options opts = GIT_SPARSE_CHECKOUT_INIT_OPTIONS_INIT;

	g_repo = cl_git_sandbox_init("sparse");

	cl_git_pass(git_sparse_checkout_set(g_repo, &initial_patterns));
	cl_git_pass(git_sparse_checkout_disable(g_repo));
	cl_git_pass(git_sparse_checkout_init(g_repo, &opts));

	cl_git_pass(git_sparse_checkout_list(&found_patterns, g_repo));
	for (i = 0; i < found_patterns.count; i++) {
		cl_assert_equal_s(found_patterns.strings[i], initial_patterns.strings[i]);
	}
}

void test_sparse_init__applies_sparsity(void)
{
	git_object* object;
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	git_sparse_checkout_init_options scopts = GIT_SPARSE_CHECKOUT_INIT_OPTIONS_INIT;

	g_repo = cl_git_sandbox_init("sparse");

	cl_git_pass(git_revparse_single(&object, g_repo, "HEAD"));
	cl_git_pass(git_checkout_tree(g_repo, object, &opts));

	cl_git_pass(git_sparse_checkout_init(g_repo, &scopts));

	cl_assert_equal_b(git_fs_path_exists("sparse/file1"), true);
	cl_assert_equal_b(git_fs_path_exists("sparse/a/file3"), false);
	cl_assert_equal_b(git_fs_path_exists("sparse/b/file5"), false);
	cl_assert_equal_b(git_fs_path_exists("sparse/b/c/file7"), false);
	cl_assert_equal_b(git_fs_path_exists("sparse/b/d/file9"), false);
}