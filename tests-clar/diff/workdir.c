#include "clar_libgit2.h"
#include "diff_helpers.h"

static git_repository *g_repo = NULL;

void test_diff_workdir__initialize(void)
{
	g_repo = cl_git_sandbox_init("status");
}

void test_diff_workdir__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

void test_diff_workdir__to_index(void)
{
	git_diff_options opts = {0};
	git_diff_list *diff = NULL;
	diff_expects exp;

	opts.context_lines = 3;
	opts.interhunk_lines = 1;
	opts.flags |= GIT_DIFF_INCLUDE_IGNORED | GIT_DIFF_INCLUDE_UNTRACKED;

	memset(&exp, 0, sizeof(exp));

	cl_git_pass(git_diff_workdir_to_index(g_repo, &opts, &diff));

	cl_git_pass(git_diff_foreach(
		diff, &exp, diff_file_fn, diff_hunk_fn, diff_line_fn));

	/* to generate these values:
	 * - cd to tests/resources/status,
	 * - mv .gitted .git
	 * - git diff --name-status
	 * - git diff
	 * - mv .git .gitted
	 */
	cl_assert_equal_i(12, exp.files);
	cl_assert_equal_i(0, exp.file_adds);
	cl_assert_equal_i(4, exp.file_dels);
	cl_assert_equal_i(4, exp.file_mods);
	cl_assert_equal_i(1, exp.file_ignored);
	cl_assert_equal_i(3, exp.file_untracked);

	cl_assert_equal_i(8, exp.hunks);

	cl_assert_equal_i(14, exp.lines);
	cl_assert_equal_i(5, exp.line_ctxt);
	cl_assert_equal_i(4, exp.line_adds);
	cl_assert_equal_i(5, exp.line_dels);

	git_diff_list_free(diff);
}

void test_diff_workdir__to_tree(void)
{
	/* grabbed a couple of commit oids from the history of the attr repo */
	const char *a_commit = "26a125ee1bf"; /* the current HEAD */
	const char *b_commit = "0017bd4ab1ec3"; /* the start */
	git_tree *a = resolve_commit_oid_to_tree(g_repo, a_commit);
	git_tree *b = resolve_commit_oid_to_tree(g_repo, b_commit);
	git_diff_options opts = {0};
	git_diff_list *diff = NULL;
	git_diff_list *diff2 = NULL;
	diff_expects exp;

	opts.context_lines = 3;
	opts.interhunk_lines = 1;
	opts.flags |= GIT_DIFF_INCLUDE_IGNORED | GIT_DIFF_INCLUDE_UNTRACKED;

	memset(&exp, 0, sizeof(exp));

	/* You can't really generate the equivalent of git_diff_workdir_to_tree()
	 * using C git.  It really wants to interpose the index into the diff.
	 *
	 * To validate the following results with command line git, I ran the
	 * following:
	 * - git ls-tree 26a125
	 * - find . ! -path ./.git/\* -a -type f | git hash-object --stdin-paths
	 * The results are documented at the bottom of this file in the
	 * long comment entitled "PREPARATION OF TEST DATA".
	 */
	cl_git_pass(git_diff_workdir_to_tree(g_repo, &opts, a, &diff));

	cl_git_pass(git_diff_foreach(
		diff, &exp, diff_file_fn, diff_hunk_fn, diff_line_fn));

	cl_assert(exp.files == 13);
	cl_assert(exp.file_adds == 0);
	cl_assert(exp.file_dels == 4);
	cl_assert(exp.file_mods == 4);
	cl_assert(exp.file_ignored == 1);
	cl_assert(exp.file_untracked == 4);

	/* Since there is no git diff equivalent, let's just assume that the
	 * text diffs produced by git_diff_foreach are accurate here.  We will
	 * do more apples-to-apples test comparison below.
	 */

	git_diff_list_free(diff);
	diff = NULL;
	memset(&exp, 0, sizeof(exp));

	/* This is a compatible emulation of "git diff <sha>" which looks like
	 * a workdir to tree diff (even though it is not really).  This is what
	 * you would get from "git diff --name-status 26a125ee1bf"
	 */
	cl_git_pass(git_diff_index_to_tree(g_repo, &opts, a, &diff));
	cl_git_pass(git_diff_workdir_to_index(g_repo, &opts, &diff2));
	cl_git_pass(git_diff_merge(diff, diff2));
	git_diff_list_free(diff2);

	cl_git_pass(git_diff_foreach(
		diff, &exp, diff_file_fn, diff_hunk_fn, diff_line_fn));

	cl_assert(exp.files == 14);
	cl_assert(exp.file_adds == 2);
	cl_assert(exp.file_dels == 5);
	cl_assert(exp.file_mods == 4);
	cl_assert(exp.file_ignored == 1);
	cl_assert(exp.file_untracked == 2);

	cl_assert(exp.hunks == 11);

	cl_assert(exp.lines == 17);
	cl_assert(exp.line_ctxt == 4);
	cl_assert(exp.line_adds == 8);
	cl_assert(exp.line_dels == 5);

	git_diff_list_free(diff);
	diff = NULL;
	memset(&exp, 0, sizeof(exp));

	/* Again, emulating "git diff <sha>" for testing purposes using
	 * "git diff --name-status 0017bd4ab1ec3" instead.
	 */
	cl_git_pass(git_diff_index_to_tree(g_repo, &opts, b, &diff));
	cl_git_pass(git_diff_workdir_to_index(g_repo, &opts, &diff2));
	cl_git_pass(git_diff_merge(diff, diff2));
	git_diff_list_free(diff2);

	cl_git_pass(git_diff_foreach(
		diff, &exp, diff_file_fn, diff_hunk_fn, diff_line_fn));

	cl_assert(exp.files == 15);
	cl_assert(exp.file_adds == 5);
	cl_assert(exp.file_dels == 4);
	cl_assert(exp.file_mods == 3);
	cl_assert(exp.file_ignored == 1);
	cl_assert(exp.file_untracked == 2);

	cl_assert(exp.hunks == 12);

	cl_assert(exp.lines == 19);
	cl_assert(exp.line_ctxt == 3);
	cl_assert(exp.line_adds == 12);
	cl_assert(exp.line_dels == 4);

	git_diff_list_free(diff);

	git_tree_free(a);
	git_tree_free(b);
}

void test_diff_workdir__to_index_with_pathspec(void)
{
	git_diff_options opts = {0};
	git_diff_list *diff = NULL;
	diff_expects exp;
	char *pathspec = NULL;

	opts.context_lines = 3;
	opts.interhunk_lines = 1;
	opts.flags |= GIT_DIFF_INCLUDE_IGNORED | GIT_DIFF_INCLUDE_UNTRACKED;
	opts.pathspec.strings = &pathspec;
	opts.pathspec.count   = 1;

	memset(&exp, 0, sizeof(exp));

	cl_git_pass(git_diff_workdir_to_index(g_repo, &opts, &diff));
	cl_git_pass(git_diff_foreach(diff, &exp, diff_file_fn, NULL, NULL));

	cl_assert_equal_i(12, exp.files);
	cl_assert_equal_i(0, exp.file_adds);
	cl_assert_equal_i(4, exp.file_dels);
	cl_assert_equal_i(4, exp.file_mods);
	cl_assert_equal_i(1, exp.file_ignored);
	cl_assert_equal_i(3, exp.file_untracked);

	git_diff_list_free(diff);

	memset(&exp, 0, sizeof(exp));
	pathspec = "modified_file";

	cl_git_pass(git_diff_workdir_to_index(g_repo, &opts, &diff));
	cl_git_pass(git_diff_foreach(diff, &exp, diff_file_fn, NULL, NULL));

	cl_assert_equal_i(1, exp.files);
	cl_assert_equal_i(0, exp.file_adds);
	cl_assert_equal_i(0, exp.file_dels);
	cl_assert_equal_i(1, exp.file_mods);
	cl_assert_equal_i(0, exp.file_ignored);
	cl_assert_equal_i(0, exp.file_untracked);

	git_diff_list_free(diff);

	memset(&exp, 0, sizeof(exp));
	pathspec = "subdir";

	cl_git_pass(git_diff_workdir_to_index(g_repo, &opts, &diff));
	cl_git_pass(git_diff_foreach(diff, &exp, diff_file_fn, NULL, NULL));

	cl_assert_equal_i(3, exp.files);
	cl_assert_equal_i(0, exp.file_adds);
	cl_assert_equal_i(1, exp.file_dels);
	cl_assert_equal_i(1, exp.file_mods);
	cl_assert_equal_i(0, exp.file_ignored);
	cl_assert_equal_i(1, exp.file_untracked);

	git_diff_list_free(diff);

	memset(&exp, 0, sizeof(exp));
	pathspec = "*_deleted";

	cl_git_pass(git_diff_workdir_to_index(g_repo, &opts, &diff));
	cl_git_pass(git_diff_foreach(diff, &exp, diff_file_fn, NULL, NULL));

	cl_assert_equal_i(2, exp.files);
	cl_assert_equal_i(0, exp.file_adds);
	cl_assert_equal_i(2, exp.file_dels);
	cl_assert_equal_i(0, exp.file_mods);
	cl_assert_equal_i(0, exp.file_ignored);
	cl_assert_equal_i(0, exp.file_untracked);

	git_diff_list_free(diff);
}

/* PREPARATION OF TEST DATA
 *
 * Since there is no command line equivalent of git_diff_workdir_to_tree,
 * it was a bit of a pain to confirm that I was getting the expected
 * results in the first part of this tests.  Here is what I ended up
 * doing to set my expectation for the file counts and results:
 *
 * Running "git ls-tree 26a125" and "git ls-tree aa27a6" shows:
 *
 *  A a0de7e0ac200c489c41c59dfa910154a70264e6e	current_file
 *  B 5452d32f1dd538eb0405e8a83cc185f79e25e80f	file_deleted
 *  C 452e4244b5d083ddf0460acf1ecc74db9dcfa11a	modified_file
 *  D 32504b727382542f9f089e24fddac5e78533e96c	staged_changes
 *  E 061d42a44cacde5726057b67558821d95db96f19	staged_changes_file_deleted
 *  F 70bd9443ada07063e7fbf0b3ff5c13f7494d89c2	staged_changes_modified_file
 *  G e9b9107f290627c04d097733a10055af941f6bca	staged_delete_file_deleted
 *  H dabc8af9bd6e9f5bbe96a176f1a24baf3d1f8916	staged_delete_modified_file
 *  I 53ace0d1cc1145a5f4fe4f78a186a60263190733	subdir/current_file
 *  J 1888c805345ba265b0ee9449b8877b6064592058	subdir/deleted_file
 *  K a6191982709b746d5650e93c2acf34ef74e11504	subdir/modified_file
 *  L e8ee89e15bbe9b20137715232387b3de5b28972e	subdir.txt
 *
 * --------
 *
 * find . ! -path ./.git/\* -a -type f | git hash-object --stdin-paths
 *
 *  A a0de7e0ac200c489c41c59dfa910154a70264e6e current_file
 *  M 6a79f808a9c6bc9531ac726c184bbcd9351ccf11 ignored_file
 *  C 0a539630525aca2e7bc84975958f92f10a64c9b6 modified_file
 *  N d4fa8600b4f37d7516bef4816ae2c64dbf029e3a new_file
 *  D 55d316c9ba708999f1918e9677d01dfcae69c6b9 staged_changes
 *  F 011c3440d5c596e21d836aa6d7b10eb581f68c49 staged_changes_modified_file
 *  H dabc8af9bd6e9f5bbe96a176f1a24baf3d1f8916 staged_delete_modified_file
 *  O 529a16e8e762d4acb7b9636ff540a00831f9155a staged_new_file
 *  P 8b090c06d14ffa09c4e880088ebad33893f921d1 staged_new_file_modified_file
 *  I 53ace0d1cc1145a5f4fe4f78a186a60263190733 subdir/current_file
 *  K 57274b75eeb5f36fd55527806d567b2240a20c57 subdir/modified_file
 *  Q 80a86a6931b91bc01c2dbf5ca55bdd24ad1ef466 subdir/new_file
 *  L e8ee89e15bbe9b20137715232387b3de5b28972e subdir.txt
 *
 * --------
 *
 *  A - current_file (UNMODIFIED) -> not in results
 *  B D file_deleted
 *  M I ignored_file (IGNORED)
 *  C M modified_file
 *  N U new_file (UNTRACKED)
 *  D M staged_changes
 *  E D staged_changes_file_deleted
 *  F M staged_changes_modified_file
 *  G D staged_delete_file_deleted
 *  H - staged_delete_modified_file (UNMODIFIED) -> not in results
 *  O U staged_new_file
 *  P U staged_new_file_modified_file
 *  I - subdir/current_file (UNMODIFIED) -> not in results
 *  J D subdir/deleted_file
 *  K M subdir/modified_file
 *  Q U subdir/new_file
 *  L - subdir.txt (UNMODIFIED) -> not in results
 *
 * Expect 13 files, 0 ADD, 4 DEL, 4 MOD, 1 IGN, 4 UNTR
 */
