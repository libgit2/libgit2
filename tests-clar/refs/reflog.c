#include "clar_libgit2.h"

#include "repository.h"
#include "git2/reflog.h"
#include "reflog.h"


static const char *new_ref = "refs/heads/test-reflog";
static const char *current_master_tip = "a65fedf39aefe402d3bb6e24df4d4f5fe4547750";
static const char *commit_msg = "commit: bla bla";

static git_repository *g_repo;


// helpers
static void assert_signature(git_signature *expected, git_signature *actual)
{
	cl_assert(actual);
	cl_assert_equal_s(expected->name, actual->name);
	cl_assert_equal_s(expected->email, actual->email);
	cl_assert(expected->when.offset == actual->when.offset);
	cl_assert(expected->when.time == actual->when.time);
}


// Fixture setup and teardown
void test_refs_reflog__initialize(void)
{
   g_repo = cl_git_sandbox_init("testrepo");
}

void test_refs_reflog__cleanup(void)
{
   cl_git_sandbox_cleanup();
}



void test_refs_reflog__write_then_read(void)
{
   // write a reflog for a given reference and ensure it can be read back
   git_repository *repo2;
	git_reference *ref, *lookedup_ref;
	git_oid oid;
	git_signature *committer;
	git_reflog *reflog;
	git_reflog_entry *entry;
	char oid_str[GIT_OID_HEXSZ+1];

	/* Create a new branch pointing at the HEAD */
	git_oid_fromstr(&oid, current_master_tip);
	cl_git_pass(git_reference_create_oid(&ref, g_repo, new_ref, &oid, 0));
	git_reference_free(ref);
	cl_git_pass(git_reference_lookup(&ref, g_repo, new_ref));

	cl_git_pass(git_signature_now(&committer, "foo", "foo@bar"));

	cl_git_pass(git_reflog_write(ref, NULL, committer, NULL));
	cl_git_fail(git_reflog_write(ref, NULL, committer, "no ancestor NULL for an existing reflog"));
	cl_git_fail(git_reflog_write(ref, NULL, committer, "no\nnewline"));
	cl_git_pass(git_reflog_write(ref, &oid, committer, commit_msg));

	/* Reopen a new instance of the repository */
	cl_git_pass(git_repository_open(&repo2, "testrepo"));

	/* Lookup the preivously created branch */
	cl_git_pass(git_reference_lookup(&lookedup_ref, repo2, new_ref));

	/* Read and parse the reflog for this branch */
	cl_git_pass(git_reflog_read(&reflog, lookedup_ref));
	cl_assert(reflog->entries.length == 2);

	entry = (git_reflog_entry *)git_vector_get(&reflog->entries, 0);
	assert_signature(committer, entry->committer);
	git_oid_tostr(oid_str, GIT_OID_HEXSZ+1, &entry->oid_old);
	cl_assert_equal_s("0000000000000000000000000000000000000000", oid_str);
	git_oid_tostr(oid_str, GIT_OID_HEXSZ+1, &entry->oid_cur);
	cl_assert_equal_s(current_master_tip, oid_str);
	cl_assert(entry->msg == NULL);

	entry = (git_reflog_entry *)git_vector_get(&reflog->entries, 1);
	assert_signature(committer, entry->committer);
	git_oid_tostr(oid_str, GIT_OID_HEXSZ+1, &entry->oid_old);
	cl_assert_equal_s(current_master_tip, oid_str);
	git_oid_tostr(oid_str, GIT_OID_HEXSZ+1, &entry->oid_cur);
	cl_assert_equal_s(current_master_tip, oid_str);
	cl_assert_equal_s(commit_msg, entry->msg);

	git_signature_free(committer);
	git_reflog_free(reflog);
	git_repository_free(repo2);

	git_reference_free(ref);
	git_reference_free(lookedup_ref);
}

void test_refs_reflog__dont_write_bad(void)
{
   // avoid writing an obviously wrong reflog
	git_reference *ref;
	git_oid oid;
	git_signature *committer;

	/* Create a new branch pointing at the HEAD */
	git_oid_fromstr(&oid, current_master_tip);
	cl_git_pass(git_reference_create_oid(&ref, g_repo, new_ref, &oid, 0));
	git_reference_free(ref);
	cl_git_pass(git_reference_lookup(&ref, g_repo, new_ref));

	cl_git_pass(git_signature_now(&committer, "foo", "foo@bar"));

	/* Write the reflog for the new branch */
	cl_git_pass(git_reflog_write(ref, NULL, committer, NULL));

	/* Try to update the reflog with wrong information:
	 * It's no new reference, so the ancestor OID cannot
	 * be NULL. */
	cl_git_fail(git_reflog_write(ref, NULL, committer, NULL));

	git_signature_free(committer);

	git_reference_free(ref);
}
