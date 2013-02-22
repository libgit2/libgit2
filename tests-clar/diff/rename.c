#include "clar_libgit2.h"
#include "diff_helpers.h"

static git_repository *g_repo = NULL;

void test_diff_rename__initialize(void)
{
	g_repo = cl_git_sandbox_init("renames");
}

void test_diff_rename__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

/*
 * Renames repo has:
 *
 * commit 31e47d8c1fa36d7f8d537b96158e3f024de0a9f2 -
 *   serving.txt     (25 lines)
 *   sevencities.txt (50 lines)
 * commit 2bc7f351d20b53f1c72c16c4b036e491c478c49a -
 *   serving.txt     -> sixserving.txt  (rename, no change, 100% match)
 *   sevencities.txt -> sevencities.txt (no change)
 *   sevencities.txt -> songofseven.txt (copy, no change, 100% match)
 * commit 1c068dee5790ef1580cfc4cd670915b48d790084
 *   songofseven.txt -> songofseven.txt (major rewrite, <20% match - split)
 *   sixserving.txt  -> sixserving.txt  (indentation change)
 *   sixserving.txt  -> ikeepsix.txt    (copy, add title, >80% match)
 *   sevencities.txt                    (no change)
 * commit 19dd32dfb1520a64e5bbaae8dce6ef423dfa2f13
 *   songofseven.txt -> untimely.txt    (rename, convert to crlf)
 *   ikeepsix.txt    -> ikeepsix.txt    (reorder sections in file)
 *   sixserving.txt  -> sixserving.txt  (whitespace change - not just indent)
 *   sevencities.txt -> songof7cities.txt (rename, small text changes)
 */

void test_diff_rename__match_oid(void)
{
	const char *old_sha = "31e47d8c1fa36d7f8d537b96158e3f024de0a9f2";
	const char *new_sha = "2bc7f351d20b53f1c72c16c4b036e491c478c49a";
	git_tree *old_tree, *new_tree;
	git_diff_list *diff;
	git_diff_options diffopts = GIT_DIFF_OPTIONS_INIT;
	git_diff_find_options opts = GIT_DIFF_FIND_OPTIONS_INIT;
	diff_expects exp;

	old_tree = resolve_commit_oid_to_tree(g_repo, old_sha);
	new_tree = resolve_commit_oid_to_tree(g_repo, new_sha);

	/* Must pass GIT_DIFF_INCLUDE_UNMODIFIED if you expect to emulate
	 * --find-copies-harder during rename transformion...
	 */
	diffopts.flags |= GIT_DIFF_INCLUDE_UNMODIFIED;

	cl_git_pass(git_diff_tree_to_tree(
		&diff, g_repo, old_tree, new_tree, &diffopts));

	/* git diff --no-renames \
	 *          31e47d8c1fa36d7f8d537b96158e3f024de0a9f2 \
	 *          2bc7f351d20b53f1c72c16c4b036e491c478c49a
	 */
	memset(&exp, 0, sizeof(exp));
	cl_git_pass(git_diff_foreach(
		diff, diff_file_cb, diff_hunk_cb, diff_line_cb, &exp));

	cl_assert_equal_i(4, exp.files);
	cl_assert_equal_i(1, exp.file_status[GIT_DELTA_UNMODIFIED]);
	cl_assert_equal_i(2, exp.file_status[GIT_DELTA_ADDED]);
	cl_assert_equal_i(1, exp.file_status[GIT_DELTA_DELETED]);

	/* git diff 31e47d8c1fa36d7f8d537b96158e3f024de0a9f2 \
	 *          2bc7f351d20b53f1c72c16c4b036e491c478c49a
	 */
	cl_git_pass(git_diff_find_similar(diff, NULL));

	memset(&exp, 0, sizeof(exp));
	cl_git_pass(git_diff_foreach(
		diff, diff_file_cb, diff_hunk_cb, diff_line_cb, &exp));

	cl_assert_equal_i(3, exp.files);
	cl_assert_equal_i(1, exp.file_status[GIT_DELTA_UNMODIFIED]);
	cl_assert_equal_i(1, exp.file_status[GIT_DELTA_ADDED]);
	cl_assert_equal_i(1, exp.file_status[GIT_DELTA_RENAMED]);

	git_diff_list_free(diff);

	cl_git_pass(git_diff_tree_to_tree(
		&diff, g_repo, old_tree, new_tree, &diffopts));

	/* git diff --find-copies-harder \
	 *          31e47d8c1fa36d7f8d537b96158e3f024de0a9f2 \
	 *          2bc7f351d20b53f1c72c16c4b036e491c478c49a
	 */
	opts.flags = GIT_DIFF_FIND_COPIES_FROM_UNMODIFIED;
	cl_git_pass(git_diff_find_similar(diff, &opts));

	memset(&exp, 0, sizeof(exp));
	cl_git_pass(git_diff_foreach(
		diff, diff_file_cb, diff_hunk_cb, diff_line_cb, &exp));

	cl_assert_equal_i(3, exp.files);
	cl_assert_equal_i(1, exp.file_status[GIT_DELTA_UNMODIFIED]);
	cl_assert_equal_i(1, exp.file_status[GIT_DELTA_COPIED]);
	cl_assert_equal_i(1, exp.file_status[GIT_DELTA_RENAMED]);

	git_diff_list_free(diff);

	git_tree_free(old_tree);
	git_tree_free(new_tree);
}

void test_diff_rename__checks_options_version(void)
{
	const char *old_sha = "31e47d8c1fa36d7f8d537b96158e3f024de0a9f2";
	const char *new_sha = "2bc7f351d20b53f1c72c16c4b036e491c478c49a";
	git_tree *old_tree, *new_tree;
	git_diff_list *diff;
	git_diff_options diffopts = GIT_DIFF_OPTIONS_INIT;
	git_diff_find_options opts = GIT_DIFF_FIND_OPTIONS_INIT;
	const git_error *err;

	old_tree = resolve_commit_oid_to_tree(g_repo, old_sha);
	new_tree = resolve_commit_oid_to_tree(g_repo, new_sha);
	diffopts.flags |= GIT_DIFF_INCLUDE_UNMODIFIED;
	cl_git_pass(git_diff_tree_to_tree(
		&diff, g_repo, old_tree, new_tree, &diffopts));

	opts.version = 0;
	cl_git_fail(git_diff_find_similar(diff, &opts));
	err = giterr_last();
	cl_assert_equal_i(GITERR_INVALID, err->klass);

	giterr_clear();
	opts.version = 1024;
	cl_git_fail(git_diff_find_similar(diff, &opts));
	err = giterr_last();
	cl_assert_equal_i(GITERR_INVALID, err->klass);

	git_diff_list_free(diff);
	git_tree_free(old_tree);
	git_tree_free(new_tree);
}

void test_diff_rename__not_exact_match(void)
{
	const char *sha0 = "2bc7f351d20b53f1c72c16c4b036e491c478c49a";
	const char *sha1 = "1c068dee5790ef1580cfc4cd670915b48d790084";
	const char *sha2 = "19dd32dfb1520a64e5bbaae8dce6ef423dfa2f13";
	git_tree *old_tree, *new_tree;
	git_diff_list *diff;
	git_diff_options diffopts = GIT_DIFF_OPTIONS_INIT;
	git_diff_find_options opts = GIT_DIFF_FIND_OPTIONS_INIT;
	diff_expects exp;

	/* == Changes =====================================================
	 * songofseven.txt -> songofseven.txt (major rewrite, <20% match - split)
	 * sixserving.txt  -> sixserving.txt  (indentation change)
	 * sixserving.txt  -> ikeepsix.txt    (copy, add title, >80% match)
	 * sevencities.txt                    (no change)
	 */

	old_tree = resolve_commit_oid_to_tree(g_repo, sha0);
	new_tree = resolve_commit_oid_to_tree(g_repo, sha1);

	/* Must pass GIT_DIFF_INCLUDE_UNMODIFIED if you expect to emulate
	 * --find-copies-harder during rename transformion...
	 */
	diffopts.flags |= GIT_DIFF_INCLUDE_UNMODIFIED;

	cl_git_pass(git_diff_tree_to_tree(
		&diff, g_repo, old_tree, new_tree, &diffopts));

	/* git diff --no-renames \
	 *          2bc7f351d20b53f1c72c16c4b036e491c478c49a \
	 *          1c068dee5790ef1580cfc4cd670915b48d790084
	 */
	memset(&exp, 0, sizeof(exp));
	cl_git_pass(git_diff_foreach(
		diff, diff_file_cb, diff_hunk_cb, diff_line_cb, &exp));

	cl_assert_equal_i(4, exp.files);
	cl_assert_equal_i(1, exp.file_status[GIT_DELTA_UNMODIFIED]);
	cl_assert_equal_i(2, exp.file_status[GIT_DELTA_MODIFIED]);
	cl_assert_equal_i(1, exp.file_status[GIT_DELTA_ADDED]);

	/* git diff -M 2bc7f351d20b53f1c72c16c4b036e491c478c49a \
	 *          1c068dee5790ef1580cfc4cd670915b48d790084
	 *
	 * must not pass NULL for opts because it will pick up environment
	 * values for "diff.renames" and test won't be consistent.
	 */
	opts.flags = GIT_DIFF_FIND_RENAMES;
	cl_git_pass(git_diff_find_similar(diff, &opts));

	memset(&exp, 0, sizeof(exp));
	cl_git_pass(git_diff_foreach(
		diff, diff_file_cb, diff_hunk_cb, diff_line_cb, &exp));

	cl_assert_equal_i(4, exp.files);
	cl_assert_equal_i(1, exp.file_status[GIT_DELTA_UNMODIFIED]);
	cl_assert_equal_i(2, exp.file_status[GIT_DELTA_MODIFIED]);
	cl_assert_equal_i(1, exp.file_status[GIT_DELTA_ADDED]);

	git_diff_list_free(diff);

	/* git diff -M -C \
	 *          2bc7f351d20b53f1c72c16c4b036e491c478c49a \
	 *          1c068dee5790ef1580cfc4cd670915b48d790084
	 */
	cl_git_pass(git_diff_tree_to_tree(
		&diff, g_repo, old_tree, new_tree, &diffopts));

	opts.flags = GIT_DIFF_FIND_RENAMES | GIT_DIFF_FIND_COPIES;
	cl_git_pass(git_diff_find_similar(diff, &opts));

	memset(&exp, 0, sizeof(exp));
	cl_git_pass(git_diff_foreach(
		diff, diff_file_cb, diff_hunk_cb, diff_line_cb, &exp));

	cl_assert_equal_i(4, exp.files);
	cl_assert_equal_i(1, exp.file_status[GIT_DELTA_UNMODIFIED]);
	cl_assert_equal_i(2, exp.file_status[GIT_DELTA_MODIFIED]);
	cl_assert_equal_i(1, exp.file_status[GIT_DELTA_COPIED]);

	git_diff_list_free(diff);

	/* git diff -M -C --find-copies-harder --break-rewrites \
	 *          2bc7f351d20b53f1c72c16c4b036e491c478c49a \
	 *          1c068dee5790ef1580cfc4cd670915b48d790084
	 */
	cl_git_pass(git_diff_tree_to_tree(
		&diff, g_repo, old_tree, new_tree, &diffopts));

	opts.flags = GIT_DIFF_FIND_ALL;
	cl_git_pass(git_diff_find_similar(diff, &opts));

	memset(&exp, 0, sizeof(exp));
	cl_git_pass(git_diff_foreach(
		diff, diff_file_cb, diff_hunk_cb, diff_line_cb, &exp));

	cl_assert_equal_i(5, exp.files);
	cl_assert_equal_i(1, exp.file_status[GIT_DELTA_UNMODIFIED]);
	cl_assert_equal_i(1, exp.file_status[GIT_DELTA_MODIFIED]);
	cl_assert_equal_i(1, exp.file_status[GIT_DELTA_DELETED]);
	cl_assert_equal_i(1, exp.file_status[GIT_DELTA_ADDED]);
	cl_assert_equal_i(1, exp.file_status[GIT_DELTA_COPIED]);

	git_diff_list_free(diff);

	/* == Changes =====================================================
	 * songofseven.txt -> untimely.txt    (rename, convert to crlf)
	 * ikeepsix.txt    -> ikeepsix.txt    (reorder sections in file)
	 * sixserving.txt  -> sixserving.txt  (whitespace - not just indent)
	 * sevencities.txt -> songof7cities.txt (rename, small text changes)
	 */

	git_tree_free(old_tree);
	old_tree = new_tree;
	new_tree = resolve_commit_oid_to_tree(g_repo, sha2);

	cl_git_pass(git_diff_tree_to_tree(
		&diff, g_repo, old_tree, new_tree, &diffopts));

	/* git diff --no-renames \
	 *          1c068dee5790ef1580cfc4cd670915b48d790084 \
	 *          19dd32dfb1520a64e5bbaae8dce6ef423dfa2f13
	 */
	memset(&exp, 0, sizeof(exp));
	cl_git_pass(git_diff_foreach(
		diff, diff_file_cb, diff_hunk_cb, diff_line_cb, &exp));

	cl_assert_equal_i(6, exp.files);
	cl_assert_equal_i(2, exp.file_status[GIT_DELTA_MODIFIED]);
	cl_assert_equal_i(2, exp.file_status[GIT_DELTA_ADDED]);
	cl_assert_equal_i(2, exp.file_status[GIT_DELTA_DELETED]);

	/* git diff -M -C \
	 *          1c068dee5790ef1580cfc4cd670915b48d790084 \
	 *          19dd32dfb1520a64e5bbaae8dce6ef423dfa2f13
	 */
	cl_git_pass(git_diff_tree_to_tree(
		&diff, g_repo, old_tree, new_tree, &diffopts));

	opts.flags = GIT_DIFF_FIND_RENAMES | GIT_DIFF_FIND_COPIES;
	cl_git_pass(git_diff_find_similar(diff, &opts));

	memset(&exp, 0, sizeof(exp));
	cl_git_pass(git_diff_foreach(
		diff, diff_file_cb, diff_hunk_cb, diff_line_cb, &exp));

	cl_assert_equal_i(4, exp.files);
	cl_assert_equal_i(2, exp.file_status[GIT_DELTA_MODIFIED]);
	cl_assert_equal_i(2, exp.file_status[GIT_DELTA_RENAMED]);

	git_diff_list_free(diff);

	/* git diff -M -C --find-copies-harder --break-rewrites \
	 *          1c068dee5790ef1580cfc4cd670915b48d790084 \
	 *          19dd32dfb1520a64e5bbaae8dce6ef423dfa2f13
	 * with libgit2 default similarity comparison...
	 */
	cl_git_pass(git_diff_tree_to_tree(
		&diff, g_repo, old_tree, new_tree, &diffopts));

	opts.flags = GIT_DIFF_FIND_ALL;
	cl_git_pass(git_diff_find_similar(diff, &opts));

	/* the default match algorithm is going to find the internal
	 * whitespace differences in the lines of sixserving.txt to be
	 * significant enough that this will decide to split it into
	 * an ADD and a DELETE
	 */

	memset(&exp, 0, sizeof(exp));
	cl_git_pass(git_diff_foreach(
		diff, diff_file_cb, diff_hunk_cb, diff_line_cb, &exp));

	cl_assert_equal_i(5, exp.files);
	cl_assert_equal_i(1, exp.file_status[GIT_DELTA_MODIFIED]);
	cl_assert_equal_i(1, exp.file_status[GIT_DELTA_ADDED]);
	cl_assert_equal_i(1, exp.file_status[GIT_DELTA_DELETED]);
	cl_assert_equal_i(2, exp.file_status[GIT_DELTA_RENAMED]);

	git_diff_list_free(diff);

	/* git diff -M -C --find-copies-harder --break-rewrites \
	 *          1c068dee5790ef1580cfc4cd670915b48d790084 \
	 *          19dd32dfb1520a64e5bbaae8dce6ef423dfa2f13
	 * with ignore_space whitespace comparision
	 */
	cl_git_pass(git_diff_tree_to_tree(
		&diff, g_repo, old_tree, new_tree, &diffopts));

	opts.flags = GIT_DIFF_FIND_ALL | GIT_DIFF_FIND_IGNORE_WHITESPACE;
	cl_git_pass(git_diff_find_similar(diff, &opts));

	/* Ignoring whitespace, this should no longer split sixserver.txt */

	memset(&exp, 0, sizeof(exp));
	cl_git_pass(git_diff_foreach(
		diff, diff_file_cb, diff_hunk_cb, diff_line_cb, &exp));

	cl_assert_equal_i(4, exp.files);
	cl_assert_equal_i(2, exp.file_status[GIT_DELTA_MODIFIED]);
	cl_assert_equal_i(2, exp.file_status[GIT_DELTA_RENAMED]);

	git_diff_list_free(diff);

	git_tree_free(old_tree);
	git_tree_free(new_tree);
}

void test_diff_rename__working_directory_changes(void)
{
	/* let's rewrite some files in the working directory on demand */

	/* and with / without CRLF changes */
}
