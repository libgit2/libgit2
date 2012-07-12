#include "clar_libgit2.h"

static git_repository *g_repo;

void test_refs_remotetracking__initialize(void)
{
	cl_git_pass(git_repository_open(&g_repo, cl_fixture("testrepo.git")));
}

void test_refs_remotetracking__cleanup(void)
{
	git_repository_free(g_repo);
}

void test_refs_remotetracking__unfound_returns_GIT_ENOTFOUND(void)
{
	git_reference *branch, *tracking;

	cl_git_pass(git_reference_lookup(&branch, g_repo, "refs/heads/subtrees"));

	cl_assert_equal_i(GIT_ENOTFOUND, git_reference_remote_tracking_from_branch(&tracking, branch));

	git_reference_free(branch);
}

void test_refs_remotetracking__retrieving_from_a_non_head_fails(void)
{
	git_reference *branch, *tracking;

	cl_git_pass(git_reference_lookup(&branch, g_repo, "refs/tags/e90810b"));

	cl_git_fail(git_reference_remote_tracking_from_branch(&tracking, branch));

	git_reference_free(branch);
}

void test_refs_remotetracking__can_retrieve_a_remote_tracking_branch_reference(void)
{
	git_reference *branch, *tracking;

	cl_git_pass(git_reference_lookup(&branch, g_repo, "refs/heads/master"));

	cl_git_pass(git_reference_remote_tracking_from_branch(&tracking, branch));

	cl_assert_equal_s("refs/remotes/test/master", git_reference_name(tracking));

	git_reference_free(branch);
	git_reference_free(tracking);
}
