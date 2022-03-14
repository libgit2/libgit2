
#include "path.h"
#include <clar_libgit2.h>
#include "futils.h"

static git_repository *g_repo = NULL;

void test_sparse_disable__initialize(void)
{
}

void test_sparse_disable__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

void test_sparse_disable__disables_sparse_checkout(void)
{
	git_config *config;
	int b;
	git_sparse_checkout_init_options scopts = GIT_SPARSE_CHECKOUT_INIT_OPTIONS_INIT;

	g_repo = cl_git_sandbox_init("sparse");

	cl_git_pass(git_sparse_checkout_init(g_repo, &scopts));
	cl_git_pass(git_sparse_checkout_disable(g_repo));

	cl_git_pass(git_repository_config(&config, g_repo));
	cl_git_pass(git_config_get_bool(&b, config, "core.sparseCheckout"));
	cl_assert_equal_b(b, false);

	git_config_free(config);
}

void test_sparse_disable__leaves_sparse_checkout_file_intact(void)
{
	const char *path;

	git_str before_content = GIT_STR_INIT;
	git_str after_content = GIT_STR_INIT;
	git_sparse_checkout_init_options scopts = GIT_SPARSE_CHECKOUT_INIT_OPTIONS_INIT;

	path = "sparse/.git/info/sparse-checkout";
	g_repo = cl_git_sandbox_init("sparse");

	cl_git_pass(git_sparse_checkout_init(g_repo, &scopts));
	cl_git_pass(git_futils_readbuffer(&before_content, path));

	cl_git_pass(git_sparse_checkout_disable(g_repo));
	cl_git_pass(git_futils_readbuffer(&after_content, path));

	cl_assert_equal_b(git_fs_path_exists(path), true);
	cl_assert_equal_s_(git_str_cstr(&before_content), git_str_cstr(&after_content), "git_sparse_checkout_disable should not modify or remove the sparse-checkout file");
}

void test_sparse_disable__restores_working_directory(void)
{
	git_sparse_checkout_init_options scopts = GIT_SPARSE_CHECKOUT_INIT_OPTIONS_INIT;
	g_repo = cl_git_sandbox_init("sparse");

	cl_git_pass(git_sparse_checkout_init(g_repo, &scopts));
	cl_git_pass(git_sparse_checkout_disable(g_repo));

	cl_assert_equal_b(git_fs_path_exists("sparse/file1"), true);
	cl_assert_equal_b(git_fs_path_exists("sparse/a/file3"), true);
	cl_assert_equal_b(git_fs_path_exists("sparse/b/file5"), true);
	cl_assert_equal_b(git_fs_path_exists("sparse/b/c/file7"), true);
	cl_assert_equal_b(git_fs_path_exists("sparse/b/d/file9"), true);
}