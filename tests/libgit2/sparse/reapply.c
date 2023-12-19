
#include <clar_libgit2.h>
#include "path.h"

static git_repository *g_repo = NULL;

void test_sparse_reapply__initialize(void)
{
}

void test_sparse_reapply__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

void rewrite_sparse_checkout_file(void)
{
	/* Manually updating the sparse-checkout file, so we don't trigger a re-apply */
	const char *path = "sparse/.git/info/sparse-checkout";
	cl_git_rewritefile(path, "/a/");
}

void test_sparse_reapply__updates_working_directory(void)
{
	git_sparse_checkout_init_options scopts = GIT_SPARSE_CHECKOUT_INIT_OPTIONS_INIT;
	g_repo = cl_git_sandbox_init("sparse");

	cl_git_pass(git_sparse_checkout_init(g_repo, &scopts));

	cl_assert_equal_b(git_fs_path_exists("sparse/file1"), true);
	cl_assert_equal_b(git_fs_path_exists("sparse/a/file3"), false);

	rewrite_sparse_checkout_file();
	cl_git_pass(git_sparse_checkout_reapply(g_repo));

	cl_assert_equal_b(git_fs_path_exists("sparse/file1"), false);
	cl_assert_equal_b(git_fs_path_exists("sparse/a/file3"), true);
}

void test_sparse_reapply__leaves_modified_files_intact(void)
{
	git_sparse_checkout_init_options scopts = GIT_SPARSE_CHECKOUT_INIT_OPTIONS_INIT;
	g_repo = cl_git_sandbox_init("sparse");

	cl_git_pass(git_sparse_checkout_init(g_repo, &scopts));

	cl_assert_equal_b(git_fs_path_exists("sparse/file1"), true);
	cl_assert_equal_b(git_fs_path_exists("sparse/a/file3"), false);

	/* Modify one of the checked out files */
	cl_git_rewritefile("sparse/file1", "what's up?");

	rewrite_sparse_checkout_file();
	cl_git_pass(git_sparse_checkout_reapply(g_repo));

	cl_assert_equal_b(git_fs_path_exists("sparse/file1"), true);
	cl_assert_equal_b(git_fs_path_exists("sparse/a/file3"), true);
}

void test_sparse_reapply__leaves_submodules_intact(void)
{
	git_submodule *sm;
	git_sparse_checkout_init_options scopts = GIT_SPARSE_CHECKOUT_INIT_OPTIONS_INIT;
	g_repo = cl_git_sandbox_init("sparse");

	cl_git_pass(git_sparse_checkout_init(g_repo, &scopts));

	cl_git_pass(git_submodule_add_setup(&sm, g_repo, "../TestGitRepository", "TestGitRepository", 1));
	git_submodule_free(sm);

	cl_assert_equal_b(git_fs_path_exists("sparse/file1"), true);
	cl_assert_equal_b(git_fs_path_exists("sparse/TestGitRepository/.git"), true);
	cl_assert_equal_b(git_fs_path_exists("sparse/a/file3"), false);

	rewrite_sparse_checkout_file();
	cl_git_pass(git_sparse_checkout_reapply(g_repo));

	cl_assert_equal_b(git_fs_path_exists("sparse/file1"), false);
	cl_assert_equal_b(git_fs_path_exists("sparse/TestGitRepository/.git"), true);
	cl_assert_equal_b(git_fs_path_exists("sparse/a/file3"), true);
}
