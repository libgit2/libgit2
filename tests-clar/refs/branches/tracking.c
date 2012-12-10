#include "clar_libgit2.h"
#include "refs.h"

static git_repository *repo;
static git_reference *branch, *tracking;

void test_refs_branches_tracking__initialize(void)
{
	cl_git_pass(git_repository_open(&repo, cl_fixture("testrepo.git")));

	branch = NULL;
	tracking = NULL;
}

void test_refs_branches_tracking__cleanup(void)
{
	git_reference_free(tracking);
	git_reference_free(branch);
	branch = NULL;

	git_repository_free(repo);
	repo = NULL;
}

void test_refs_branches_tracking__can_retrieve_the_remote_tracking_reference_of_a_local_branch(void)
{
	cl_git_pass(git_reference_lookup(&branch, repo, "refs/heads/master"));

	cl_git_pass(git_branch_tracking(&tracking, branch));

	cl_assert_equal_s("refs/remotes/test/master", git_reference_name(tracking));
}

void test_refs_branches_tracking__can_retrieve_the_local_tracking_reference_of_a_local_branch(void)
{
	cl_git_pass(git_reference_lookup(&branch, repo, "refs/heads/track-local"));

	cl_git_pass(git_branch_tracking(&tracking, branch));

	cl_assert_equal_s("refs/heads/master", git_reference_name(tracking));
}

void test_refs_branches_tracking__cannot_retrieve_a_remote_tracking_reference_from_a_non_branch(void)
{
	cl_git_pass(git_reference_lookup(&branch, repo, "refs/tags/e90810b"));

	cl_git_fail(git_branch_tracking(&tracking, branch));
}

void test_refs_branches_tracking__trying_to_retrieve_a_remote_tracking_reference_from_a_plain_local_branch_returns_GIT_ENOTFOUND(void)
{
	cl_git_pass(git_reference_lookup(&branch, repo, "refs/heads/subtrees"));

	cl_assert_equal_i(GIT_ENOTFOUND, git_branch_tracking(&tracking, branch));
}

void test_refs_branches_tracking__trying_to_retrieve_a_remote_tracking_reference_from_a_branch_with_no_fetchspec_returns_GIT_ENOTFOUND(void)
{
	cl_git_pass(git_reference_lookup(&branch, repo, "refs/heads/cannot-fetch"));

	cl_assert_equal_i(GIT_ENOTFOUND, git_branch_tracking(&tracking, branch));
}

static void assert_merge_and_or_remote_key_missing(git_repository *repository, const git_commit *target, const char *entry_name)
{
	git_reference *branch;

	cl_assert_equal_i(GIT_OBJ_COMMIT, git_object_type((git_object*)target));
	cl_git_pass(git_branch_create(&branch, repository, entry_name, (git_commit*)target, 0));

	cl_assert_equal_i(GIT_ENOTFOUND, git_branch_tracking(&tracking, branch));

	git_reference_free(branch);
}

void test_refs_branches_tracking__retrieve_a_remote_tracking_reference_from_a_branch_with_no_remote_returns_GIT_ENOTFOUND(void)
{
	git_reference *head;
	git_repository *repository;
	git_commit *target;

	repository = cl_git_sandbox_init("testrepo.git");

	cl_git_pass(git_repository_head(&head, repository));
	cl_git_pass(git_reference_peel(((git_object **)&target), head, GIT_OBJ_COMMIT));
	git_reference_free(head);

	assert_merge_and_or_remote_key_missing(repository, target, "remoteless");
	assert_merge_and_or_remote_key_missing(repository, target, "mergeless");
	assert_merge_and_or_remote_key_missing(repository, target, "mergeandremoteless");

	git_commit_free(target);

	cl_git_sandbox_cleanup();
}
