#include "clar_libgit2.h"
#include "refs.h"

static git_repository *repo;
static git_reference *fake_remote;

void test_refs_branches_delete__initialize(void)
{
	git_oid id;

	cl_fixture_sandbox("testrepo.git");
	cl_git_pass(git_repository_open(&repo, "testrepo.git"));

	cl_git_pass(git_oid_fromstr(&id, "be3563ae3f795b2b4353bcce3a527ad0a4f7f644"));
	cl_git_pass(git_reference_create_oid(&fake_remote, repo, "refs/remotes/nulltoken/master", &id, 0));
}

void test_refs_branches_delete__cleanup(void)
{
	git_reference_free(fake_remote);
	git_repository_free(repo);

	cl_fixture_cleanup("testrepo.git");
}

void test_refs_branches_delete__can_not_delete_a_branch_pointed_at_by_HEAD(void)
{
	git_reference *head;
	git_reference *branch;

	/* Ensure HEAD targets the local master branch */
	cl_git_pass(git_reference_lookup(&head, repo, GIT_HEAD_FILE));
	cl_assert(strcmp("refs/heads/master", git_reference_target(head)) == 0);
	git_reference_free(head);

	cl_git_pass(git_branch_lookup(&branch, repo, "master", GIT_BRANCH_LOCAL));
	cl_git_fail(git_branch_delete(branch));
	git_reference_free(branch);
}

void test_refs_branches_delete__can_not_delete_a_branch_if_HEAD_is_missing(void)
{
	git_reference *head;
	git_reference *branch = NULL;

	cl_git_pass(git_reference_lookup(&head, repo, GIT_HEAD_FILE));
	git_reference_delete(head);

	cl_git_pass(git_branch_lookup(&branch, repo, "br2", GIT_BRANCH_LOCAL));
	cl_git_fail(git_branch_delete(branch));
	git_reference_free(branch);
}

void test_refs_branches_delete__can_delete_a_branch_pointed_at_by_detached_HEAD(void)
{
	git_reference *master, *head, *branch;

	/* Detach HEAD and make it target the commit that "master" points to */
	cl_git_pass(git_reference_lookup(&master, repo, "refs/heads/master"));
	cl_git_pass(git_reference_create_oid(&head, repo, "HEAD", git_reference_oid(master), 1));
	git_reference_free(head);
	git_reference_free(master);

	cl_git_pass(git_branch_lookup(&branch, repo, "master", GIT_BRANCH_LOCAL));
	cl_git_pass(git_branch_delete(branch));
}

void test_refs_branches_delete__can_delete_a_local_branch(void)
{
	git_reference *branch;
	cl_git_pass(git_branch_lookup(&branch, repo, "br2", GIT_BRANCH_LOCAL));
	cl_git_pass(git_branch_delete(branch));
}

void test_refs_branches_delete__can_delete_a_remote_branch(void)
{
	git_reference *branch;
	cl_git_pass(git_branch_lookup(&branch, repo, "nulltoken/master", GIT_BRANCH_REMOTE));
	cl_git_pass(git_branch_delete(branch));
}

