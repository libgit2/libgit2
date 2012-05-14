#include "clar_libgit2.h"
#include "branch.h"

static git_repository *repo;

void test_refs_branches_move__initialize(void)
{
	cl_fixture_sandbox("testrepo.git");
	cl_git_pass(git_repository_open(&repo, "testrepo.git"));
}

void test_refs_branches_move__cleanup(void)
{
	git_repository_free(repo);

	cl_fixture_cleanup("testrepo.git");
}

#define NEW_BRANCH_NAME "new-branch-on-the-block"

void test_refs_branches_move__can_move_a_local_branch(void)
{
	cl_git_pass(git_branch_move(repo, "br2", NEW_BRANCH_NAME, 0));
}

void test_refs_branches_move__can_move_a_local_branch_to_a_different_namespace(void)
{
	/* Downward */
	cl_git_pass(git_branch_move(repo, "br2", "somewhere/" NEW_BRANCH_NAME, 0));

	/* Upward */
	cl_git_pass(git_branch_move(repo, "somewhere/" NEW_BRANCH_NAME, "br2", 0));
}

void test_refs_branches_move__can_move_a_local_branch_to_a_partially_colliding_namespace(void)
{
	/* Downward */
	cl_git_pass(git_branch_move(repo, "br2", "br2/" NEW_BRANCH_NAME, 0));

	/* Upward */
	cl_git_pass(git_branch_move(repo, "br2/" NEW_BRANCH_NAME, "br2", 0));
}

void test_refs_branches_move__can_not_move_a_branch_if_its_destination_name_collide_with_an_existing_one(void)
{
	cl_git_fail(git_branch_move(repo, "br2", "master", 0));
}

void test_refs_branches_move__can_not_move_a_non_existing_branch(void)
{
	cl_git_fail(git_branch_move(repo, "i-am-no-branch", NEW_BRANCH_NAME, 0));
}

void test_refs_branches_move__can_force_move_over_an_existing_branch(void)
{
	cl_git_pass(git_branch_move(repo, "br2", "master", 1));
}

void test_refs_branches_move__can_not_move_a_branch_through_its_canonical_name(void)
{
	cl_git_fail(git_branch_move(repo, "refs/heads/br2", NEW_BRANCH_NAME, 1));
}

void test_refs_branches_move__moving_a_non_exisiting_branch_returns_ENOTFOUND(void)
{
	int error;

	error = git_branch_move(repo, "where/am/I", NEW_BRANCH_NAME, 0);
	cl_git_fail(error);

	cl_assert_equal_i(GIT_ENOTFOUND, error);
}
