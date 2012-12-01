#include "clar_libgit2.h"
#include "refs.h"
#include "config/config_helpers.h"

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
	ref = NULL;

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
	cl_assert_equal_i(GIT_EEXISTS, git_branch_move(ref, "master", 0));
}

void test_refs_branches_move__moving_a_branch_with_an_invalid_name_returns_EINVALIDSPEC(void)
{
	cl_assert_equal_i(GIT_EINVALIDSPEC, git_branch_move(ref, "Inv@{id", 0));
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

void test_refs_branches_move__moving_a_branch_moves_related_configuration_data(void)
{
	git_reference *branch;

	cl_git_pass(git_branch_lookup(&branch, repo, "track-local", GIT_BRANCH_LOCAL));

	assert_config_entry_existence(repo, "branch.track-local.remote", true);
	assert_config_entry_existence(repo, "branch.track-local.merge", true);
	assert_config_entry_existence(repo, "branch.moved.remote", false);
	assert_config_entry_existence(repo, "branch.moved.merge", false);

	cl_git_pass(git_branch_move(branch, "moved", 0));

	assert_config_entry_existence(repo, "branch.track-local.remote", false);
	assert_config_entry_existence(repo, "branch.track-local.merge", false);
	assert_config_entry_existence(repo, "branch.moved.remote", true);
	assert_config_entry_existence(repo, "branch.moved.merge", true);

	git_reference_free(branch);
}

void test_refs_branches_move__moving_the_branch_pointed_at_by_HEAD_updates_HEAD(void)
{
	git_reference *branch;

	cl_git_pass(git_reference_lookup(&branch, repo, "refs/heads/master"));
	cl_git_pass(git_branch_move(branch, "master2", 0));
	git_reference_free(branch);

	cl_git_pass(git_repository_head(&branch, repo));
	cl_assert_equal_s("refs/heads/master2", git_reference_name(branch));
	git_reference_free(branch);
}
