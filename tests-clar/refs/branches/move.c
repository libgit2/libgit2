#include "clar_libgit2.h"
#include "refs.h"

static git_repository *repo;
static git_reference *ref;

void test_refs_branches_move__initialize(void)
{
	repo = cl_git_sandbox_init("testrepo.git");

	cl_git_pass(git_reference_lookup(&ref, repo, "refs/heads/br2"));
}

void test_refs_branches_move__cleanup(void)
{
	git_reference_free(ref);
	cl_git_sandbox_cleanup();
}

#define NEW_BRANCH_NAME "new-branch-on-the-block"

void test_refs_branches_move__can_move_a_local_branch(void)
{
	cl_git_pass(git_branch_move(ref, NEW_BRANCH_NAME, 0));
	cl_assert_equal_s(GIT_REFS_HEADS_DIR NEW_BRANCH_NAME, git_reference_name(ref));
}

void test_refs_branches_move__can_move_a_local_branch_to_a_different_namespace(void)
{
	/* Downward */
	cl_git_pass(git_branch_move(ref, "somewhere/" NEW_BRANCH_NAME, 0));

	/* Upward */
	cl_git_pass(git_branch_move(ref, "br2", 0));
}

void test_refs_branches_move__can_move_a_local_branch_to_a_partially_colliding_namespace(void)
{
	/* Downward */
	cl_git_pass(git_branch_move(ref, "br2/" NEW_BRANCH_NAME, 0));

	/* Upward */
	cl_git_pass(git_branch_move(ref, "br2", 0));
}

void test_refs_branches_move__can_not_move_a_branch_if_its_destination_name_collide_with_an_existing_one(void)
{
	cl_git_fail(git_branch_move(ref, "master", 0));
}

void test_refs_branches_move__can_not_move_a_non_branch(void)
{
	git_reference *tag;

	cl_git_pass(git_reference_lookup(&tag, repo, "refs/tags/e90810b"));
	cl_git_fail(git_branch_move(tag, NEW_BRANCH_NAME, 0));

	git_reference_free(tag);
}

void test_refs_branches_move__can_force_move_over_an_existing_branch(void)
{
	cl_git_pass(git_branch_move(ref, "master", 1));
}
