#include "clar_libgit2.h"
#include "diff_helpers.h"

static git_repository *g_repo = NULL;

void test_diff_index__initialize(void)
{
	g_repo = cl_git_sandbox_init("status");
}

void test_diff_index__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

void test_diff_index__0(void)
{
	/* grabbed a couple of commit oids from the history of the attr repo */
	const char *a_commit = "26a125ee1bf"; /* the current HEAD */
	const char *b_commit = "0017bd4ab1ec3"; /* the start */
	git_tree *a = resolve_commit_oid_to_tree(g_repo, a_commit);
	git_tree *b = resolve_commit_oid_to_tree(g_repo, b_commit);
	git_diff_options opts = {0};
	git_diff_list *diff = NULL;
	diff_expects exp;

	cl_assert(a);
	cl_assert(b);

	opts.context_lines = 1;
	opts.interhunk_lines = 1;

	memset(&exp, 0, sizeof(exp));

	cl_git_pass(git_diff_index_to_tree(g_repo, &opts, a, &diff));

	cl_git_pass(git_diff_foreach(
		diff, &exp, diff_file_fn, diff_hunk_fn, diff_line_fn));

	/* to generate these values:
	 * - cd to tests/resources/status,
	 * - mv .gitted .git
	 * - git diff --name-status --cached 26a125ee1bf
	 * - git diff -U1 --cached 26a125ee1bf
	 * - mv .git .gitted
	 */
	cl_assert(exp.files == 8);
	cl_assert(exp.file_adds == 3);
	cl_assert(exp.file_dels == 2);
	cl_assert(exp.file_mods == 3);

	cl_assert(exp.hunks == 8);

	cl_assert(exp.lines == 11);
	cl_assert(exp.line_ctxt == 3);
	cl_assert(exp.line_adds == 6);
	cl_assert(exp.line_dels == 2);

	git_diff_list_free(diff);
	diff = NULL;
	memset(&exp, 0, sizeof(exp));

	cl_git_pass(git_diff_index_to_tree(g_repo, &opts, b, &diff));

	cl_git_pass(git_diff_foreach(
		diff, &exp, diff_file_fn, diff_hunk_fn, diff_line_fn));

	/* to generate these values:
	 * - cd to tests/resources/status,
	 * - mv .gitted .git
	 * - git diff --name-status --cached 0017bd4ab1ec3
	 * - git diff -U1 --cached 0017bd4ab1ec3
	 * - mv .git .gitted
	 */
	cl_assert(exp.files == 12);
	cl_assert(exp.file_adds == 7);
	cl_assert(exp.file_dels == 2);
	cl_assert(exp.file_mods == 3);

	cl_assert(exp.hunks == 12);

	cl_assert(exp.lines == 16);
	cl_assert(exp.line_ctxt == 3);
	cl_assert(exp.line_adds == 11);
	cl_assert(exp.line_dels == 2);

	git_diff_list_free(diff);
	diff = NULL;

	git_tree_free(a);
	git_tree_free(b);
}
