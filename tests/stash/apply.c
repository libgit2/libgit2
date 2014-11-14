#include "clar_libgit2.h"
#include "fileops.h"
#include "stash_helpers.h"

static git_signature *signature;
static git_repository *repo;
static git_index *repo_index;

void test_stash_apply__initialize(void)
{
	git_oid oid;

	cl_git_pass(git_signature_new(&signature, "nulltoken", "emeric.fermas@gmail.com", 1323847743, 60)); /* Wed Dec 14 08:29:03 2011 +0100 */

	cl_git_pass(git_repository_init(&repo, "stash", 0));
	cl_git_pass(git_repository_index(&repo_index, repo));

	cl_git_mkfile("stash/what", "hello\n");
	cl_git_mkfile("stash/how", "small\n");
	cl_git_mkfile("stash/who", "world\n");

	cl_git_pass(git_index_add_bypath(repo_index, "what"));
	cl_git_pass(git_index_add_bypath(repo_index, "how"));
	cl_git_pass(git_index_add_bypath(repo_index, "who"));

	cl_repo_commit_from_index(NULL, repo, signature, 0, "Initial commit");

	cl_git_rewritefile("stash/what", "goodbye\n");
	cl_git_rewritefile("stash/who", "funky world\n");
	cl_git_mkfile("stash/when", "tomorrow\n");

	cl_git_pass(git_index_add_bypath(repo_index, "who"));

	/* Pre-stash state */
	assert_status(repo, "what", GIT_STATUS_WT_MODIFIED);
	assert_status(repo, "how", GIT_STATUS_CURRENT);
	assert_status(repo, "who", GIT_STATUS_INDEX_MODIFIED);
	assert_status(repo, "when", GIT_STATUS_WT_NEW);

	cl_git_pass(git_stash_save(&oid, repo, signature, NULL, GIT_STASH_INCLUDE_UNTRACKED));

	/* Post-stash state */
	assert_status(repo, "what", GIT_STATUS_CURRENT);
	assert_status(repo, "how", GIT_STATUS_CURRENT);
	assert_status(repo, "who", GIT_STATUS_CURRENT);
	assert_status(repo, "when", GIT_ENOTFOUND);
}

void test_stash_apply__cleanup(void)
{
	git_signature_free(signature);
	signature = NULL;

	git_index_free(repo_index);
	repo_index = NULL;

	git_repository_free(repo);
	repo = NULL;

	cl_git_pass(git_futils_rmdir_r("stash", NULL, GIT_RMDIR_REMOVE_FILES));
	cl_fixture_cleanup("sorry-it-is-a-non-bare-only-party");
}

void test_stash_apply__with_default(void)
{
	cl_git_pass(git_stash_apply(repo, 0, GIT_APPLY_DEFAULT));

	cl_assert_equal_i(git_index_has_conflicts(repo_index), 0);
	assert_status(repo, "what", GIT_STATUS_WT_MODIFIED);
	assert_status(repo, "how", GIT_STATUS_CURRENT);
	assert_status(repo, "who", GIT_STATUS_WT_MODIFIED);
	assert_status(repo, "when", GIT_STATUS_WT_NEW);
}

void test_stash_apply__with_reinstate_index(void)
{
	cl_git_pass(git_stash_apply(repo, 0, GIT_APPLY_REINSTATE_INDEX));

	cl_assert_equal_i(git_index_has_conflicts(repo_index), 0);
	assert_status(repo, "what", GIT_STATUS_WT_MODIFIED);
	assert_status(repo, "how", GIT_STATUS_CURRENT);
	assert_status(repo, "who", GIT_STATUS_INDEX_MODIFIED);
	assert_status(repo, "when", GIT_STATUS_WT_NEW);
}

void test_stash_apply__conflict_index_with_default(void)
{
	const git_index_entry *ancestor;
	const git_index_entry *our;
	const git_index_entry *their;

	cl_git_rewritefile("stash/who", "nothing\n");
	cl_git_pass(git_index_add_bypath(repo_index, "who"));
	cl_git_pass(git_index_write(repo_index));

	cl_git_pass(git_stash_apply(repo, 0, GIT_APPLY_DEFAULT));

	cl_assert_equal_i(git_index_has_conflicts(repo_index), 1);
	assert_status(repo, "what", GIT_STATUS_INDEX_MODIFIED);
	assert_status(repo, "how", GIT_STATUS_CURRENT);
	cl_git_pass(git_index_conflict_get(&ancestor, &our, &their, repo_index, "who")); /* unmerged */
	assert_status(repo, "when", GIT_STATUS_WT_NEW);
}

void test_stash_apply__conflict_index_with_reinstate_index(void)
{
	cl_git_rewritefile("stash/who", "nothing\n");
	cl_git_pass(git_index_add_bypath(repo_index, "who"));
	cl_git_pass(git_index_write(repo_index));

	cl_git_fail_with(git_stash_apply(repo, 0, GIT_APPLY_REINSTATE_INDEX), GIT_EUNMERGED);

	cl_assert_equal_i(git_index_has_conflicts(repo_index), 0);
	assert_status(repo, "what", GIT_STATUS_CURRENT);
	assert_status(repo, "how", GIT_STATUS_CURRENT);
	assert_status(repo, "who", GIT_STATUS_INDEX_MODIFIED);
	assert_status(repo, "when", GIT_ENOTFOUND);
}

void test_stash_apply__conflict_untracked_with_default(void)
{
	cl_git_mkfile("stash/when", "nothing\n");

	cl_git_fail_with(git_stash_apply(repo, 0, GIT_APPLY_DEFAULT), GIT_EEXISTS);

	cl_assert_equal_i(git_index_has_conflicts(repo_index), 0);
	assert_status(repo, "what", GIT_STATUS_CURRENT);
	assert_status(repo, "how", GIT_STATUS_CURRENT);
	assert_status(repo, "who", GIT_STATUS_CURRENT);
	assert_status(repo, "when", GIT_STATUS_WT_NEW);
}

void test_stash_apply__conflict_untracked_with_reinstate_index(void)
{
	cl_git_mkfile("stash/when", "nothing\n");

	cl_git_fail_with(git_stash_apply(repo, 0, GIT_APPLY_REINSTATE_INDEX), GIT_EEXISTS);

	cl_assert_equal_i(git_index_has_conflicts(repo_index), 0);
	assert_status(repo, "what", GIT_STATUS_CURRENT);
	assert_status(repo, "how", GIT_STATUS_CURRENT);
	assert_status(repo, "who", GIT_STATUS_CURRENT);
	assert_status(repo, "when", GIT_STATUS_WT_NEW);
}

void test_stash_apply__conflict_workdir_with_default(void)
{
	cl_git_rewritefile("stash/what", "ciao\n");

	cl_git_fail_with(git_stash_apply(repo, 0, GIT_APPLY_DEFAULT), GIT_EMERGECONFLICT);

	cl_assert_equal_i(git_index_has_conflicts(repo_index), 0);
	assert_status(repo, "what", GIT_STATUS_WT_MODIFIED);
	assert_status(repo, "how", GIT_STATUS_CURRENT);
	assert_status(repo, "who", GIT_STATUS_CURRENT);
	assert_status(repo, "when", GIT_STATUS_WT_NEW);
}

void test_stash_apply__conflict_workdir_with_reinstate_index(void)
{
	cl_git_rewritefile("stash/what", "ciao\n");

	cl_git_fail_with(git_stash_apply(repo, 0, GIT_APPLY_REINSTATE_INDEX), GIT_EMERGECONFLICT);

	cl_assert_equal_i(git_index_has_conflicts(repo_index), 0);
	assert_status(repo, "what", GIT_STATUS_WT_MODIFIED);
	assert_status(repo, "how", GIT_STATUS_CURRENT);
	assert_status(repo, "who", GIT_STATUS_CURRENT);
	assert_status(repo, "when", GIT_STATUS_WT_NEW);
}

void test_stash_apply__conflict_commit_with_default(void)
{
	const git_index_entry *ancestor;
	const git_index_entry *our;
	const git_index_entry *their;

	cl_git_rewritefile("stash/what", "ciao\n");
	cl_git_pass(git_index_add_bypath(repo_index, "what"));
	cl_repo_commit_from_index(NULL, repo, signature, 0, "Other commit");

	cl_git_pass(git_stash_apply(repo, 0, GIT_APPLY_DEFAULT));

	cl_assert_equal_i(git_index_has_conflicts(repo_index), 1);
	cl_git_pass(git_index_conflict_get(&ancestor, &our, &their, repo_index, "what")); /* unmerged */
	assert_status(repo, "how", GIT_STATUS_CURRENT);
	assert_status(repo, "who", GIT_STATUS_INDEX_MODIFIED);
	assert_status(repo, "when", GIT_STATUS_WT_NEW);
}

void test_stash_apply__conflict_commit_with_reinstate_index(void)
{
	const git_index_entry *ancestor;
	const git_index_entry *our;
	const git_index_entry *their;

	cl_git_rewritefile("stash/what", "ciao\n");
	cl_git_pass(git_index_add_bypath(repo_index, "what"));
	cl_repo_commit_from_index(NULL, repo, signature, 0, "Other commit");

	cl_git_pass(git_stash_apply(repo, 0, GIT_APPLY_REINSTATE_INDEX));

	cl_assert_equal_i(git_index_has_conflicts(repo_index), 1);
	cl_git_pass(git_index_conflict_get(&ancestor, &our, &their, repo_index, "what")); /* unmerged */
	assert_status(repo, "how", GIT_STATUS_CURRENT);
	assert_status(repo, "who", GIT_STATUS_INDEX_MODIFIED);
	assert_status(repo, "when", GIT_STATUS_WT_NEW);
}

void test_stash_apply__pop(void)
{
	cl_git_pass(git_stash_pop(repo, 0, GIT_APPLY_DEFAULT));

	cl_git_fail_with(git_stash_pop(repo, 0, GIT_APPLY_DEFAULT), GIT_ENOTFOUND);
}
