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
 *
 * TODO: add commits with various % changes of copy / rename
 */

void test_diff_rename__match_oid(void)
{
	const char *old_sha = "31e47d8c1fa36d7f8d537b96158e3f024de0a9f2";
	const char *new_sha = "2bc7f351d20b53f1c72c16c4b036e491c478c49a";
	git_tree *old_tree, *new_tree;
	git_diff_list *diff;
	git_diff_options diffopts = {0};
	git_diff_find_options opts;
	diff_expects exp;

	old_tree = resolve_commit_oid_to_tree(g_repo, old_sha);
	new_tree = resolve_commit_oid_to_tree(g_repo, new_sha);

	/* Must pass GIT_DIFF_INCLUDE_UNMODIFIED if you expect to emulate
	 * --find-copies-harder during rename transformion...
	 */
	memset(&diffopts, 0, sizeof(diffopts));
	diffopts.flags |= GIT_DIFF_INCLUDE_UNMODIFIED;

	cl_git_pass(git_diff_tree_to_tree(
		&diff, g_repo, old_tree, new_tree, &diffopts));

	/* git diff --no-renames \
	 *          31e47d8c1fa36d7f8d537b96158e3f024de0a9f2 \
	 *          2bc7f351d20b53f1c72c16c4b036e491c478c49a
	 */
	memset(&exp, 0, sizeof(exp));
	cl_git_pass(git_diff_foreach(
		diff, &exp, diff_file_cb, diff_hunk_cb, diff_line_cb));

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
		diff, &exp, diff_file_cb, diff_hunk_cb, diff_line_cb));

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
	memset(&opts, 0, sizeof(opts));
	opts.flags = GIT_DIFF_FIND_COPIES_FROM_UNMODIFIED;
	cl_git_pass(git_diff_find_similar(diff, &opts));

	memset(&exp, 0, sizeof(exp));
	cl_git_pass(git_diff_foreach(
		diff, &exp, diff_file_cb, diff_hunk_cb, diff_line_cb));

	cl_assert_equal_i(3, exp.files);
	cl_assert_equal_i(1, exp.file_status[GIT_DELTA_UNMODIFIED]);
	cl_assert_equal_i(1, exp.file_status[GIT_DELTA_COPIED]);
	cl_assert_equal_i(1, exp.file_status[GIT_DELTA_RENAMED]);

	git_diff_list_free(diff);

	git_tree_free(old_tree);
	git_tree_free(new_tree);
}
