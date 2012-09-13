#include "clar_libgit2.h"
#include "refs.h"

static git_repository *repo;
static git_reference *branch;

void test_refs_branches_tracking__initialize(void)
{
	cl_git_pass(git_repository_open(&repo, cl_fixture("testrepo.git")));

	branch = NULL;
}

void test_refs_branches_tracking__cleanup(void)
{
	git_reference_free(branch);

	git_repository_free(repo);
}

void test_refs_branches_tracking__can_retrieve_the_remote_tracking_reference_of_a_local_branch(void)
{
	git_reference *branch, *tracking;

	cl_git_pass(git_reference_lookup(&branch, repo, "refs/heads/master"));

	cl_git_pass(git_branch_tracking(&tracking, branch));

	cl_assert_equal_s("refs/remotes/test/master", git_reference_name(tracking));

	git_reference_free(branch);
	git_reference_free(tracking);
}

void test_refs_branches_tracking__can_retrieve_the_local_tracking_reference_of_a_local_branch(void)
{
	git_reference *branch, *tracking;

	cl_git_pass(git_reference_lookup(&branch, repo, "refs/heads/track-local"));

	cl_git_pass(git_branch_tracking(&tracking, branch));

	cl_assert_equal_s("refs/heads/master", git_reference_name(tracking));

	git_reference_free(branch);
	git_reference_free(tracking);
}

void test_refs_branches_tracking__cannot_retrieve_a_remote_tracking_reference_from_a_non_branch(void)
{
	git_reference *branch, *tracking;

	cl_git_pass(git_reference_lookup(&branch, repo, "refs/tags/e90810b"));

	cl_git_fail(git_branch_tracking(&tracking, branch));

	git_reference_free(branch);
}

void test_refs_branches_tracking__trying_to_retrieve_a_remote_tracking_reference_from_a_plain_local_branch_returns_GIT_ENOTFOUND(void)
{
	git_reference *branch, *tracking;

	cl_git_pass(git_reference_lookup(&branch, repo, "refs/heads/subtrees"));

	cl_assert_equal_i(GIT_ENOTFOUND, git_branch_tracking(&tracking, branch));

	git_reference_free(branch);
}

void test_refs_branches_tracking__trying_to_retrieve_a_remote_tracking_reference_from_a_branch_with_no_fetchspec_returns_GIT_ENOTFOUND(void)
{
	git_reference *branch, *tracking;

	cl_git_pass(git_reference_lookup(&branch, repo, "refs/heads/cannot-fetch"));

	cl_assert_equal_i(GIT_ENOTFOUND, git_branch_tracking(&tracking, branch));

	git_reference_free(branch);
}
