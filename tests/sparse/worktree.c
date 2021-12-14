
#include "repository.h"
#include "clar_libgit2.h"

static git_repository *g_repo = NULL;

void test_sparse_worktree__initialize(void)
{
}

void test_sparse_worktree__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

void test_sparse_worktree__honours_sparsity(void)
{
	git_str path = GIT_STR_INIT;
	git_worktree *wt;
	git_worktree_add_options opts = GIT_WORKTREE_ADD_OPTIONS_INIT;

	git_repository *wt_repo;
	git_sparse_checkout_init_options scopts = GIT_SPARSE_CHECKOUT_INIT_OPTIONS_INIT;

	g_repo = cl_git_sandbox_init("sparse");

	cl_git_pass(git_str_joinpath(&path, g_repo->workdir, "../sparse-worktree"));
	cl_git_pass(git_worktree_add(&wt, g_repo, "sparse-worktree", path.ptr, &opts));
	cl_git_pass(git_repository_open(&wt_repo, path.ptr));

	cl_git_pass(git_sparse_checkout_init(&scopts, wt_repo));

	cl_assert_equal_b(git_fs_path_exists("sparse-worktree/file1"), true);
	cl_assert_equal_b(git_fs_path_exists("sparse-worktree/a/file3"), false);
	cl_assert_equal_b(git_fs_path_exists("sparse-worktree/b/file5"), false);
	cl_assert_equal_b(git_fs_path_exists("sparse-worktree/b/c/file7"), false);
	cl_assert_equal_b(git_fs_path_exists("sparse-worktree/b/d/file9"), false);
}

void test_sparse_worktree__honours_sparsity_on_different_worktrees(void)
{
	git_str path1 = GIT_STR_INIT;
	git_worktree *wt1;
	git_repository *wt_repo1;
	git_worktree_add_options opts = GIT_WORKTREE_ADD_OPTIONS_INIT;

	char* pattern_strings1[] = { "/a/" };
	git_strarray patterns1 = { pattern_strings1, ARRAY_SIZE(pattern_strings1) };

	git_str path2 = GIT_STR_INIT;
	git_worktree *wt2;
	git_repository *wt_repo2;

	char* pattern_strings2[] = { "/b/" };
	git_strarray patterns2 = { pattern_strings2, ARRAY_SIZE(pattern_strings2) };

	g_repo = cl_git_sandbox_init("sparse");

	cl_git_pass(git_str_joinpath(&path1, g_repo->workdir, "../sparse-worktree-1"));
	cl_git_pass(git_worktree_add(&wt1, g_repo, "sparse-worktree-1", path1.ptr, &opts));
	cl_git_pass(git_repository_open(&wt_repo1, path1.ptr));

	cl_git_pass(git_str_joinpath(&path2, g_repo->workdir, "../sparse-worktree-2"));
	cl_git_pass(git_worktree_add(&wt2, g_repo, "sparse-worktree-2", path2.ptr, &opts));
	cl_git_pass(git_repository_open(&wt_repo2, path2.ptr));

	cl_git_pass(git_sparse_checkout_set(&patterns1, wt_repo1));

	cl_assert_equal_b(git_fs_path_exists("sparse-worktree-1/file1"), false);
	cl_assert_equal_b(git_fs_path_exists("sparse-worktree-1/a/file3"), true);
	cl_assert_equal_b(git_fs_path_exists("sparse-worktree-1/b/file5"), false);

	cl_git_pass(git_sparse_checkout_set(&patterns2, wt_repo2));

	cl_assert_equal_b(git_fs_path_exists("sparse-worktree-2/file1"), false);
	cl_assert_equal_b(git_fs_path_exists("sparse-worktree-2/a/file3"), false);
	cl_assert_equal_b(git_fs_path_exists("sparse-worktree-2/b/file5"), true);
}
