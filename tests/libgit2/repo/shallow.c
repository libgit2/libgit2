#include "clar_libgit2.h"
#include "futils.h"

#include "git2/worktree.h"

static git_repository *g_repo;

void test_repo_shallow__initialize(void)
{
}

void test_repo_shallow__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

void test_repo_shallow__no_shallow_file(void)
{
	g_repo = cl_git_sandbox_init("testrepo.git");
	cl_assert_equal_i(0, git_repository_is_shallow(g_repo));
}

void test_repo_shallow__empty_shallow_file(void)
{
	g_repo = cl_git_sandbox_init("testrepo.git");
	cl_git_mkfile("testrepo.git/shallow", "");
	cl_assert_equal_i(0, git_repository_is_shallow(g_repo));
}

void test_repo_shallow__shallow_repo(void)
{
	g_repo = cl_git_sandbox_init("shallow.git");
	cl_assert_equal_i(1, git_repository_is_shallow(g_repo));
}

void test_repo_shallow__clears_errors(void)
{
	g_repo = cl_git_sandbox_init("testrepo.git");
	cl_assert_equal_i(0, git_repository_is_shallow(g_repo));
	cl_assert_equal_i(GIT_ERROR_NONE, git_error_last()->klass);
}

void test_repo_shallow__worktree_of_shallow_parent(void)
{
	git_worktree *wt;
	git_repository *wt_repo;
	git_str path = GIT_STR_INIT;

	g_repo = cl_git_sandbox_init("testrepo");

	/* any non-empty content marks the parent as shallow */
	cl_git_mkfile("testrepo/.git/shallow", "x");

	cl_git_pass(git_str_joinpath(&path, git_repository_workdir(g_repo), "../wt"));
	cl_git_pass(git_worktree_add(&wt, g_repo, "wt", path.ptr, NULL));
	cl_git_pass(git_repository_open_from_worktree(&wt_repo, wt));

	cl_assert_equal_i(1, git_repository_is_shallow(wt_repo));

	git_repository_free(wt_repo);
	git_worktree_free(wt);
	git_str_dispose(&path);
}
