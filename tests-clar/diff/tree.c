#include "clar_libgit2.h"
#include "diff_helpers.h"

static git_repository *g_repo = NULL;

void test_diff_tree__initialize(void)
{
}

void test_diff_tree__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

void test_diff_tree__0(void)
{
	/* grabbed a couple of commit oids from the history of the attr repo */
	const char *a_commit = "605812a";
	const char *b_commit = "370fe9ec22";
	const char *c_commit = "f5b0af1fb4f5c";
	git_tree *a, *b, *c;
	git_diff_options opts = {0};
	git_diff_list *diff = NULL;
	diff_expects exp;

	g_repo = cl_git_sandbox_init("attr");

	cl_assert((a = resolve_commit_oid_to_tree(g_repo, a_commit)) != NULL);
	cl_assert((b = resolve_commit_oid_to_tree(g_repo, b_commit)) != NULL);
	cl_assert((c = resolve_commit_oid_to_tree(g_repo, c_commit)) != NULL);

	opts.context_lines = 1;
	opts.interhunk_lines = 1;

	memset(&exp, 0, sizeof(exp));

	cl_git_pass(git_diff_tree_to_tree(g_repo, &opts, a, b, &diff));

	cl_git_pass(git_diff_foreach(
		diff, &exp, diff_file_fn, diff_hunk_fn, diff_line_fn));

	cl_assert(exp.files == 5);
	cl_assert(exp.file_adds == 2);
	cl_assert(exp.file_dels == 1);
	cl_assert(exp.file_mods == 2);

	cl_assert(exp.hunks == 5);

	cl_assert(exp.lines == 7 + 24 + 1 + 6 + 6);
	cl_assert(exp.line_ctxt == 1);
	cl_assert(exp.line_adds == 24 + 1 + 5 + 5);
	cl_assert(exp.line_dels == 7 + 1);

	git_diff_list_free(diff);
	diff = NULL;

	memset(&exp, 0, sizeof(exp));

	cl_git_pass(git_diff_tree_to_tree(g_repo, &opts, c, b, &diff));

	cl_git_pass(git_diff_foreach(
		diff, &exp, diff_file_fn, diff_hunk_fn, diff_line_fn));

	cl_assert(exp.files == 2);
	cl_assert(exp.file_adds == 0);
	cl_assert(exp.file_dels == 0);
	cl_assert(exp.file_mods == 2);

	cl_assert(exp.hunks == 2);

	cl_assert(exp.lines == 8 + 15);
	cl_assert(exp.line_ctxt == 1);
	cl_assert(exp.line_adds == 1);
	cl_assert(exp.line_dels == 7 + 14);

	git_diff_list_free(diff);

	git_tree_free(a);
	git_tree_free(b);
	git_tree_free(c);
}

void test_diff_tree__options(void)
{
	/* grabbed a couple of commit oids from the history of the attr repo */
	const char *a_commit = "6bab5c79cd5140d0";
	const char *b_commit = "605812ab7fe421fdd";
	const char *c_commit = "f5b0af1fb4f5";
	const char *d_commit = "a97cc019851";
	git_tree *a, *b, *c, *d;
	git_diff_options opts = {0};
	git_diff_list *diff = NULL;
	diff_expects actual;
	int test_ab_or_cd[] = { 0, 0, 0, 0, 1, 1, 1, 1, 1 };
	git_diff_options test_options[] = {
		/* a vs b tests */
		{ GIT_DIFF_NORMAL, 1, 1, NULL, NULL, {0} },
		{ GIT_DIFF_NORMAL, 3, 1, NULL, NULL, {0} },
		{ GIT_DIFF_REVERSE, 2, 1, NULL, NULL, {0} },
		{ GIT_DIFF_FORCE_TEXT, 2, 1, NULL, NULL, {0} },
		/* c vs d tests */
		{ GIT_DIFF_NORMAL, 3, 1, NULL, NULL, {0} },
		{ GIT_DIFF_IGNORE_WHITESPACE, 3, 1, NULL, NULL, {0} },
		{ GIT_DIFF_IGNORE_WHITESPACE_CHANGE, 3, 1, NULL, NULL, {0} },
		{ GIT_DIFF_IGNORE_WHITESPACE_EOL, 3, 1, NULL, NULL, {0} },
		{ GIT_DIFF_IGNORE_WHITESPACE | GIT_DIFF_REVERSE, 1, 1, NULL, NULL, {0} },
	};
	/* to generate these values:
	 * - cd to tests/resources/attr,
	 * - mv .gitted .git
	 * - git diff [options] 6bab5c79cd5140d0 605812ab7fe421fdd
	 * - mv .git .gitted
	 */
	diff_expects test_expects[] = {
		/* a vs b tests */
		{ 5, 3, 0, 2, 0, 0, 0, 4, 0, 0, 51, 2, 46, 3 },
		{ 5, 3, 0, 2, 0, 0, 0, 4, 0, 0, 53, 4, 46, 3 },
		{ 5, 0, 3, 2, 0, 0, 0, 4, 0, 0, 52, 3, 3, 46 },
		{ 5, 3, 0, 2, 0, 0, 0, 5, 0, 0, 54, 3, 48, 3 },
		/* c vs d tests */
		{ 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 22, 9, 10, 3 },
		{ 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 19, 12, 7, 0 },
		{ 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 20, 11, 8, 1 },
		{ 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 20, 11, 8, 1 },
		{ 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 18, 11, 0, 7 },
		{ 0 },
	};
	diff_expects *expected;
	int i;

	g_repo = cl_git_sandbox_init("attr");

	cl_assert((a = resolve_commit_oid_to_tree(g_repo, a_commit)) != NULL);
	cl_assert((b = resolve_commit_oid_to_tree(g_repo, b_commit)) != NULL);
	cl_assert((c = resolve_commit_oid_to_tree(g_repo, c_commit)) != NULL);
	cl_assert((d = resolve_commit_oid_to_tree(g_repo, d_commit)) != NULL);

	for (i = 0; test_expects[i].files > 0; i++) {
		memset(&actual, 0, sizeof(actual)); /* clear accumulator */
		opts = test_options[i];

		if (test_ab_or_cd[i] == 0)
			cl_git_pass(git_diff_tree_to_tree(g_repo, &opts, a, b, &diff));
		else
			cl_git_pass(git_diff_tree_to_tree(g_repo, &opts, c, d, &diff));

		cl_git_pass(git_diff_foreach(
			diff, &actual, diff_file_fn, diff_hunk_fn, diff_line_fn));

		expected = &test_expects[i];
		cl_assert_equal_i(actual.files,     expected->files);
		cl_assert_equal_i(actual.file_adds, expected->file_adds);
 		cl_assert_equal_i(actual.file_dels, expected->file_dels);
		cl_assert_equal_i(actual.file_mods, expected->file_mods);
		cl_assert_equal_i(actual.hunks,     expected->hunks);
		cl_assert_equal_i(actual.lines,     expected->lines);
		cl_assert_equal_i(actual.line_ctxt, expected->line_ctxt);
		cl_assert_equal_i(actual.line_adds, expected->line_adds);
		cl_assert_equal_i(actual.line_dels, expected->line_dels);

		git_diff_list_free(diff);
		diff = NULL;
	}

	git_tree_free(a);
	git_tree_free(b);
	git_tree_free(c);
	git_tree_free(d);
}

void test_diff_tree__bare(void)
{
	const char *a_commit = "8496071c1b46c85";
	const char *b_commit = "be3563ae3f79";
	git_tree *a, *b;
	git_diff_options opts = {0};
	git_diff_list *diff = NULL;
	diff_expects exp;

	g_repo = cl_git_sandbox_init("testrepo.git");

	cl_assert((a = resolve_commit_oid_to_tree(g_repo, a_commit)) != NULL);
	cl_assert((b = resolve_commit_oid_to_tree(g_repo, b_commit)) != NULL);

	opts.context_lines = 1;
	opts.interhunk_lines = 1;

	memset(&exp, 0, sizeof(exp));

	cl_git_pass(git_diff_tree_to_tree(g_repo, &opts, a, b, &diff));

	cl_git_pass(git_diff_foreach(
		diff, &exp, diff_file_fn, diff_hunk_fn, diff_line_fn));

	cl_assert(exp.files == 3);
	cl_assert(exp.file_adds == 2);
	cl_assert(exp.file_dels == 0);
	cl_assert(exp.file_mods == 1);

	cl_assert(exp.hunks == 3);

	cl_assert(exp.lines == 4);
	cl_assert(exp.line_ctxt == 0);
	cl_assert(exp.line_adds == 3);
	cl_assert(exp.line_dels == 1);

	git_diff_list_free(diff);
	git_tree_free(a);
	git_tree_free(b);
}
