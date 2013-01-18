#include "clar_libgit2.h"
#include "fileops.h"
#include "stash_helpers.h"

static git_repository *repo;
static git_signature *signature;

void test_stash_drop__initialize(void)
{
	cl_git_pass(git_repository_init(&repo, "stash", 0));
	cl_git_pass(git_signature_new(&signature, "nulltoken", "emeric.fermas@gmail.com", 1323847743, 60)); /* Wed Dec 14 08:29:03 2011 +0100 */
}

void test_stash_drop__cleanup(void)
{
	git_signature_free(signature);
	signature = NULL;

	git_repository_free(repo);
	repo = NULL;

	cl_git_pass(git_futils_rmdir_r("stash", NULL, GIT_RMDIR_REMOVE_FILES));
}

void test_stash_drop__cannot_drop_from_an_empty_stash(void)
{
	cl_assert_equal_i(GIT_ENOTFOUND, git_stash_drop(repo, 0));
}

static void push_three_states(void)
{
	git_oid oid;
	git_index *index;

	cl_git_mkfile("stash/zero.txt", "content\n");
	cl_git_pass(git_repository_index(&index, repo));
	cl_git_pass(git_index_add_bypath(index, "zero.txt"));
	commit_staged_files(&oid, index, signature);
	cl_assert(git_path_exists("stash/zero.txt"));

	cl_git_mkfile("stash/one.txt", "content\n");
	cl_git_pass(git_stash_save(&oid, repo, signature, "First", GIT_STASH_INCLUDE_UNTRACKED));
	cl_assert(!git_path_exists("stash/one.txt"));
	cl_assert(git_path_exists("stash/zero.txt"));

	cl_git_mkfile("stash/two.txt", "content\n");
	cl_git_pass(git_stash_save(&oid, repo, signature, "Second", GIT_STASH_INCLUDE_UNTRACKED));
	cl_assert(!git_path_exists("stash/two.txt"));
	cl_assert(git_path_exists("stash/zero.txt"));

	cl_git_mkfile("stash/three.txt", "content\n");
	cl_git_pass(git_stash_save(&oid, repo, signature, "Third", GIT_STASH_INCLUDE_UNTRACKED));
	cl_assert(!git_path_exists("stash/three.txt"));
	cl_assert(git_path_exists("stash/zero.txt"));

	git_index_free(index);
}

void test_stash_drop__cannot_drop_a_non_existing_stashed_state(void)
{
	push_three_states();

	cl_assert_equal_i(GIT_ENOTFOUND, git_stash_drop(repo, 666));
	cl_assert_equal_i(GIT_ENOTFOUND, git_stash_drop(repo, 42));
	cl_assert_equal_i(GIT_ENOTFOUND, git_stash_drop(repo, 3));
}

void test_stash_drop__can_purge_the_stash_from_the_top(void)
{
	push_three_states();

	cl_git_pass(git_stash_drop(repo, 0));
	cl_git_pass(git_stash_drop(repo, 0));
	cl_git_pass(git_stash_drop(repo, 0));

	cl_assert_equal_i(GIT_ENOTFOUND, git_stash_drop(repo, 0));
}

void test_stash_drop__can_purge_the_stash_from_the_bottom(void)
{
	push_three_states();

	cl_git_pass(git_stash_drop(repo, 2));
	cl_git_pass(git_stash_drop(repo, 1));
	cl_git_pass(git_stash_drop(repo, 0));

	cl_assert_equal_i(GIT_ENOTFOUND, git_stash_drop(repo, 0));
}

void test_stash_drop__dropping_an_entry_rewrites_reflog_history(void)
{
	git_reference *stash;
	git_reflog *reflog;
	const git_reflog_entry *entry;
	git_oid oid;
	size_t count;

	push_three_states();

	cl_git_pass(git_reference_lookup(&stash, repo, "refs/stash"));

	cl_git_pass(git_reflog_read(&reflog, stash));
	entry = git_reflog_entry_byindex(reflog, 1);

	git_oid_cpy(&oid, git_reflog_entry_id_old(entry));
	count = git_reflog_entrycount(reflog);

	git_reflog_free(reflog);

	cl_git_pass(git_stash_drop(repo, 1));

	cl_git_pass(git_reflog_read(&reflog, stash));
	entry = git_reflog_entry_byindex(reflog, 0);

	cl_assert_equal_i(0, git_oid_cmp(&oid, git_reflog_entry_id_old(entry)));
	cl_assert_equal_sz(count - 1, git_reflog_entrycount(reflog));

	git_reflog_free(reflog);

	git_reference_free(stash);
}

void test_stash_drop__dropping_the_last_entry_removes_the_stash(void)
{
	git_reference *stash;

	push_three_states();

	cl_git_pass(git_reference_lookup(&stash, repo, "refs/stash"));
	git_reference_free(stash);

	cl_git_pass(git_stash_drop(repo, 0));
	cl_git_pass(git_stash_drop(repo, 0));
	cl_git_pass(git_stash_drop(repo, 0));

	cl_assert_equal_i(GIT_ENOTFOUND, git_reference_lookup(&stash, repo, "refs/stash"));
}
