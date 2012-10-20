#include "clar_libgit2.h"
#include "reset_helpers.h"
#include "repo/repo_helpers.h"

static git_repository *repo;
static git_object *target;

void test_reset_soft__initialize(void)
{
	repo = cl_git_sandbox_init("testrepo.git");
}

void test_reset_soft__cleanup(void)
{
	git_object_free(target);
	cl_git_sandbox_cleanup();
}

static void assert_reset_soft(bool should_be_detached)
{
	git_oid oid;

	cl_git_pass(git_reference_name_to_oid(&oid, repo, "HEAD"));
	cl_git_fail(git_oid_streq(&oid, KNOWN_COMMIT_IN_BARE_REPO));

	retrieve_target_from_oid(&target, repo, KNOWN_COMMIT_IN_BARE_REPO);

	cl_assert(git_repository_head_detached(repo) == should_be_detached);

	cl_git_pass(git_reset(repo, target, GIT_RESET_SOFT));

	cl_assert(git_repository_head_detached(repo) == should_be_detached);

	cl_git_pass(git_reference_name_to_oid(&oid, repo, "HEAD"));
	cl_git_pass(git_oid_streq(&oid, KNOWN_COMMIT_IN_BARE_REPO));
}

void test_reset_soft__can_reset_the_non_detached_Head_to_the_specified_commit(void)
{
	assert_reset_soft(false);
}

void test_reset_soft__can_reset_the_detached_Head_to_the_specified_commit(void)
{
	git_repository_detach_head(repo);

	assert_reset_soft(true);
}

void test_reset_soft__resetting_to_the_commit_pointed_at_by_the_Head_does_not_change_the_target_of_the_Head(void)
{
	git_oid oid;
	char raw_head_oid[GIT_OID_HEXSZ + 1];

	cl_git_pass(git_reference_name_to_oid(&oid, repo, "HEAD"));
	git_oid_fmt(raw_head_oid, &oid);
	raw_head_oid[GIT_OID_HEXSZ] = '\0';

	retrieve_target_from_oid(&target, repo, raw_head_oid);

	cl_git_pass(git_reset(repo, target, GIT_RESET_SOFT));

	cl_git_pass(git_reference_name_to_oid(&oid, repo, "HEAD"));
	cl_git_pass(git_oid_streq(&oid, raw_head_oid));
}

void test_reset_soft__resetting_to_a_tag_sets_the_Head_to_the_peeled_commit(void)
{
	git_oid oid;

	/* b25fa35 is a tag, pointing to another tag which points to commit e90810b */
	retrieve_target_from_oid(&target, repo, "b25fa35b38051e4ae45d4222e795f9df2e43f1d1");

	cl_git_pass(git_reset(repo, target, GIT_RESET_SOFT));

	cl_assert(git_repository_head_detached(repo) == false);
	cl_git_pass(git_reference_name_to_oid(&oid, repo, "HEAD"));
	cl_git_pass(git_oid_streq(&oid, KNOWN_COMMIT_IN_BARE_REPO));
}

void test_reset_soft__cannot_reset_to_a_tag_not_pointing_at_a_commit(void)
{
	/* 53fc32d is the tree of commit e90810b */
	retrieve_target_from_oid(&target, repo, "53fc32d17276939fc79ed05badaef2db09990016");

	cl_git_fail(git_reset(repo, target, GIT_RESET_SOFT));
	git_object_free(target);

	/* 521d87c is an annotated tag pointing to a blob */
	retrieve_target_from_oid(&target, repo, "521d87c1ec3aef9824daf6d96cc0ae3710766d91");
	cl_git_fail(git_reset(repo, target, GIT_RESET_SOFT));
}

void test_reset_soft__resetting_against_an_orphaned_head_repo_makes_the_head_no_longer_orphaned(void)
{
	git_reference *head;

	retrieve_target_from_oid(&target, repo, KNOWN_COMMIT_IN_BARE_REPO);

	make_head_orphaned(repo, NON_EXISTING_HEAD);

	cl_assert_equal_i(true, git_repository_head_orphan(repo));

	cl_git_pass(git_reset(repo, target, GIT_RESET_SOFT));

	cl_assert_equal_i(false, git_repository_head_orphan(repo));

	cl_git_pass(git_reference_lookup(&head, repo, NON_EXISTING_HEAD));
	cl_assert_equal_i(0, git_oid_streq(git_reference_oid(head), KNOWN_COMMIT_IN_BARE_REPO));

	git_reference_free(head);
}
