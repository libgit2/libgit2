#include "clar_libgit2.h"
#include "worktree_helpers.h"

#define WORKTREE_PARENT "submodules-worktree-parent"
#define WORKTREE_CHILD "submodules-worktree-child"

void test_worktree_open__repository(void)
{
	worktree_fixture fixture =
		WORKTREE_FIXTURE_INIT("testrepo", "testrepo-worktree");
	setup_fixture_worktree(&fixture);

	cl_assert(git_repository_path(fixture.worktree) != NULL);
	cl_assert(git_repository_workdir(fixture.worktree) != NULL);

	cleanup_fixture_worktree(&fixture);
}

void test_worktree_open__repository_with_nonexistent_parent(void)
{
	git_repository *repo;

	cl_fixture_sandbox("testrepo-worktree");
	cl_git_pass(p_chdir("testrepo-worktree"));
	cl_git_pass(cl_rename(".gitted", ".git"));
	cl_git_pass(p_chdir(".."));

	cl_git_fail(git_repository_open(&repo, "testrepo-worktree"));

	cl_fixture_cleanup("testrepo-worktree");
}

void test_worktree_open__submodule_worktree_parent(void)
{
	worktree_fixture fixture =
		WORKTREE_FIXTURE_INIT("submodules", WORKTREE_PARENT);
	setup_fixture_worktree(&fixture);

	cl_assert(git_repository_path(fixture.worktree) != NULL);
	cl_assert(git_repository_workdir(fixture.worktree) != NULL);

	cleanup_fixture_worktree(&fixture);
}

void test_worktree_open__submodule_worktree_child(void)
{
	worktree_fixture parent_fixture =
		WORKTREE_FIXTURE_INIT("submodules", WORKTREE_PARENT);
	worktree_fixture child_fixture =
		WORKTREE_FIXTURE_INIT(NULL, WORKTREE_CHILD);

	setup_fixture_worktree(&parent_fixture);
	cl_git_pass(p_rename(
		"submodules/testrepo/.gitted",
		"submodules/testrepo/.git"));
	setup_fixture_worktree(&child_fixture);

	cleanup_fixture_worktree(&child_fixture);
	cleanup_fixture_worktree(&parent_fixture);
}
