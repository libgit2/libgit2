#include "clar_libgit2.h"
#include "refs.h"

static git_repository *repo;
static git_commit *target;
static git_reference *branch;

void test_refs_branches_create__initialize(void)
{
	cl_fixture_sandbox("testrepo.git");
	cl_git_pass(git_repository_open(&repo, "testrepo.git"));

	branch = NULL;
}

void test_refs_branches_create__cleanup(void)
{
	git_reference_free(branch);
	branch = NULL;

	git_commit_free(target);
	target = NULL;

	git_repository_free(repo);
	repo = NULL;

	cl_fixture_cleanup("testrepo.git");
}

static void retrieve_target_from_oid(git_commit **out, git_repository *repo, const char *sha)
{
	git_oid oid;

	cl_git_pass(git_oid_fromstr(&oid, sha));
	cl_git_pass(git_commit_lookup(out, repo, &oid));
}

static void retrieve_known_commit(git_commit **commit, git_repository *repo)
{
	retrieve_target_from_oid(commit, repo, "e90810b8df3e80c413d903f631643c716887138d");
}

#define NEW_BRANCH_NAME "new-branch-on-the-block"

void test_refs_branches_create__can_create_a_local_branch(void)
{
	retrieve_known_commit(&target, repo);

	cl_git_pass(git_branch_create(&branch, repo, NEW_BRANCH_NAME, target, 0));
	cl_git_pass(git_oid_cmp(git_reference_target(branch), git_commit_id(target)));
}

void test_refs_branches_create__can_not_create_a_branch_if_its_name_collide_with_an_existing_one(void)
{
	retrieve_known_commit(&target, repo);

	cl_assert_equal_i(GIT_EEXISTS, git_branch_create(&branch, repo, "br2", target, 0));
}

void test_refs_branches_create__can_force_create_over_an_existing_branch(void)
{
	retrieve_known_commit(&target, repo);

	cl_git_pass(git_branch_create(&branch, repo, "br2", target, 1));
	cl_git_pass(git_oid_cmp(git_reference_target(branch), git_commit_id(target)));
	cl_assert_equal_s("refs/heads/br2", git_reference_name(branch));
}


void test_refs_branches_create__creating_a_branch_with_an_invalid_name_returns_EINVALIDSPEC(void)
{
	retrieve_known_commit(&target, repo);

	cl_assert_equal_i(GIT_EINVALIDSPEC,
		git_branch_create(&branch, repo, "inv@{id", target, 0));
}