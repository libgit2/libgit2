#include "clar_libgit2.h"
#include "refs.h"

static git_repository *repo;
static git_object *target;
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

	git_object_free(target);
	git_repository_free(repo);

	cl_fixture_cleanup("testrepo.git");
}

static void retrieve_target_from_oid(git_object **object_out, git_repository *repo, const char *sha)
{
	git_oid oid;

	cl_git_pass(git_oid_fromstr(&oid, sha));
	cl_git_pass(git_object_lookup(object_out, repo, &oid, GIT_OBJ_ANY));
}

static void retrieve_known_commit(git_object **object, git_repository *repo)
{
	retrieve_target_from_oid(object, repo, "e90810b8df3e80c413d903f631643c716887138d");
}

#define NEW_BRANCH_NAME "new-branch-on-the-block"

void test_refs_branches_create__can_create_a_local_branch(void)
{
	retrieve_known_commit(&target, repo);

	cl_git_pass(git_branch_create(&branch, repo, NEW_BRANCH_NAME, target, 0));
	cl_git_pass(git_oid_cmp(git_reference_oid(branch), git_object_id(target)));
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
	cl_git_pass(git_oid_cmp(git_reference_oid(branch), git_object_id(target)));
	cl_assert_equal_s("refs/heads/br2", git_reference_name(branch));
}

void test_refs_branches_create__creating_a_branch_targeting_a_tag_dereferences_it_to_its_commit(void)
{
	/* b25fa35 is a tag, pointing to another tag which points to a commit */
	retrieve_target_from_oid(&target, repo, "b25fa35b38051e4ae45d4222e795f9df2e43f1d1");

	cl_git_pass(git_branch_create(&branch, repo, NEW_BRANCH_NAME, target, 0));
	cl_git_pass(git_oid_streq(git_reference_oid(branch), "e90810b8df3e80c413d903f631643c716887138d"));
}

void test_refs_branches_create__can_not_create_a_branch_pointing_to_a_non_commit_object(void)
{
	/* 53fc32d is the tree of commit e90810b */
	retrieve_target_from_oid(&target, repo, "53fc32d17276939fc79ed05badaef2db09990016");

	cl_git_fail(git_branch_create(&branch, repo, NEW_BRANCH_NAME, target, 0));
	git_object_free(target);

	/* 521d87c is an annotated tag pointing to a blob */
	retrieve_target_from_oid(&target, repo, "521d87c1ec3aef9824daf6d96cc0ae3710766d91");

	cl_git_fail(git_branch_create(&branch, repo, NEW_BRANCH_NAME, target, 0));
}
