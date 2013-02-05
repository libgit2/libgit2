#include "clar_libgit2.h"
#include "branch.h"

static git_repository *repo;
static git_buf tracking_name;

void test_refs_branches_trackingname__initialize(void)
{
	cl_git_pass(git_repository_open(&repo, cl_fixture("testrepo.git")));

	git_buf_init(&tracking_name, 0);
}

void test_refs_branches_trackingname__cleanup(void)
{
	git_buf_free(&tracking_name);

	git_repository_free(repo);
	repo = NULL;
}

void test_refs_branches_trackingname__can_retrieve_the_remote_tracking_reference_name_of_a_local_branch(void)
{
	cl_git_pass(git_branch_tracking__name(
		&tracking_name, repo, "refs/heads/master"));

	cl_assert_equal_s("refs/remotes/test/master", git_buf_cstr(&tracking_name));
}

void test_refs_branches_trackingname__can_retrieve_the_local_tracking_reference_name_of_a_local_branch(void)
{
	cl_git_pass(git_branch_tracking__name(
		&tracking_name, repo, "refs/heads/track-local"));

	cl_assert_equal_s("refs/heads/master", git_buf_cstr(&tracking_name));
}

void test_refs_branches_trackingname__can_return_the_size_of_thelocal_tracking_reference_name_of_a_local_branch(void)
{
	cl_assert_equal_i((int)strlen("refs/heads/master") + 1,
		git_branch_tracking_name(NULL, 0, repo, "refs/heads/track-local"));
}
