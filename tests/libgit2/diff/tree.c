#include "clar_libgit2.h"
#include "diff_helpers.h"

static git_repository *g_repo = NULL;
static git_diff_options opts;
static git_diff *diff;
static git_tree *a, *b;
static diff_expects expect;

void test_diff_tree__initialize(void)
{
	cl_git_pass(git_diff_options_init(&opts, GIT_DIFF_OPTIONS_VERSION));

	memset(&expect, 0, sizeof(expect));

	diff = NULL;
	a = NULL;
	b = NULL;
}

void test_diff_tree__cleanup(void)
{
	git_diff_free(diff);
	git_tree_free(a);
	git_tree_free(b);

	cl_git_sandbox_cleanup();

}

void test_diff_tree__0(void)
{
	/* grabbed a couple of commit oids from the history of the attr repo */
	const char *a_commit = "605812a";
	const char *b_commit = "370fe9ec22";
	const char *c_commit = "f5b0af1fb4f5c";
	git_tree *c;

	g_repo = cl_git_sandbox_init("attr");

	cl_assert((a = resolve_commit_oid_to_tree(g_repo, a_commit)) != NULL);
	cl_assert((b = resolve_commit_oid_to_tree(g_repo, b_commit)) != NULL);
	cl_assert((c = resolve_commit_oid_to_tree(g_repo, c_commit)) != NULL);

	opts.context_lines = 1;
	opts.interhunk_lines = 1;


	cl_git_pass(git_diff_tree_to_tree(&diff, g_repo, a, b, &opts));

	cl_git_pass(git_diff_foreach(
		diff, diff_file_cb, diff_binary_cb, diff_hunk_cb, diff_line_cb, &expect));

	cl_assert_equal_i(5, expect.files);
	cl_assert_equal_i(2, expect.file_status[GIT_DELTA_ADDED]);
	cl_assert_equal_i(1, expect.file_status[GIT_DELTA_DELETED]);
	cl_assert_equal_i(2, expect.file_status[GIT_DELTA_MODIFIED]);

	cl_assert_equal_i(5, expect.hunks);

	cl_assert_equal_i(7 + 24 + 1 + 6 + 6, expect.lines);
	cl_assert_equal_i(1, expect.line_ctxt);
	cl_assert_equal_i(24 + 1 + 5 + 5, expect.line_adds);
	cl_assert_equal_i(7 + 1, expect.line_dels);

	git_diff_free(diff);
	diff = NULL;

	memset(&expect, 0, sizeof(expect));

	cl_git_pass(git_diff_tree_to_tree(&diff, g_repo, c, b, &opts));

	cl_git_pass(git_diff_foreach(
		diff, diff_file_cb, diff_binary_cb, diff_hunk_cb, diff_line_cb, &expect));

	cl_assert_equal_i(2, expect.files);
	cl_assert_equal_i(0, expect.file_status[GIT_DELTA_ADDED]);
	cl_assert_equal_i(0, expect.file_status[GIT_DELTA_DELETED]);
	cl_assert_equal_i(2, expect.file_status[GIT_DELTA_MODIFIED]);

	cl_assert_equal_i(2, expect.hunks);

	cl_assert_equal_i(8 + 15, expect.lines);
	cl_assert_equal_i(1, expect.line_ctxt);
	cl_assert_equal_i(1, expect.line_adds);
	cl_assert_equal_i(7 + 14, expect.line_dels);

	git_tree_free(c);
}

#define DIFF_OPTS(FLAGS, CTXT) \
	{GIT_DIFF_OPTIONS_VERSION, (FLAGS), GIT_SUBMODULE_IGNORE_UNSPECIFIED, \
	{NULL,0}, NULL, NULL, NULL, (CTXT), 1}

void test_diff_tree__options(void)
{
	/* grabbed a couple of commit oids from the history of the attr repo */
	const char *a_commit = "6bab5c79cd5140d0";
	const char *b_commit = "605812ab7fe421fdd";
	const char *c_commit = "f5b0af1fb4f5";
	const char *d_commit = "a97cc019851";
	git_tree *c, *d;
	diff_expects actual;
	int test_ab_or_cd[] = { 0, 0, 0, 0, 1, 1, 1, 1, 1 };
	git_diff_options test_options[] = {
		/* a vs b tests */
		DIFF_OPTS(GIT_DIFF_NORMAL, 1),
		DIFF_OPTS(GIT_DIFF_NORMAL, 3),
		DIFF_OPTS(GIT_DIFF_REVERSE, 2),
		DIFF_OPTS(GIT_DIFF_FORCE_TEXT, 2),
		/* c vs d tests */
		DIFF_OPTS(GIT_DIFF_NORMAL, 3),
		DIFF_OPTS(GIT_DIFF_IGNORE_WHITESPACE, 3),
		DIFF_OPTS(GIT_DIFF_IGNORE_WHITESPACE_CHANGE, 3),
		DIFF_OPTS(GIT_DIFF_IGNORE_WHITESPACE_EOL, 3),
		DIFF_OPTS(GIT_DIFF_IGNORE_WHITESPACE | GIT_DIFF_REVERSE, 1),
	};

	/* to generate these values:
	 * - cd to tests/resources/attr,
	 * - mv .gitted .git
	 * - git diff [options] 6bab5c79cd5140d0 605812ab7fe421fdd
	 * - mv .git .gitted
	 */
#define EXPECT_STATUS_ADM(ADDS,DELS,MODS) { 0, ADDS, DELS, MODS, 0, 0, 0, 0, 0 }

	diff_expects test_expects[] = {
		/* a vs b tests */
		{ 5, 0, EXPECT_STATUS_ADM(3, 0, 2), 4, 0, 0, 51, 2, 46, 3 },
		{ 5, 0, EXPECT_STATUS_ADM(3, 0, 2), 4, 0, 0, 53, 4, 46, 3 },
		{ 5, 0, EXPECT_STATUS_ADM(0, 3, 2), 4, 0, 0, 52, 3, 3, 46 },
		{ 5, 0, EXPECT_STATUS_ADM(3, 0, 2), 5, 0, 0, 54, 3, 47, 4 },
		/* c vs d tests */
		{ 1, 0, EXPECT_STATUS_ADM(0, 0, 1), 1, 0, 0, 22, 9, 10, 3 },
		{ 1, 0, EXPECT_STATUS_ADM(0, 0, 1), 1, 0, 0, 19, 12, 7, 0 },
		{ 1, 0, EXPECT_STATUS_ADM(0, 0, 1), 1, 0, 0, 20, 11, 8, 1 },
		{ 1, 0, EXPECT_STATUS_ADM(0, 0, 1), 1, 0, 0, 20, 11, 8, 1 },
		{ 1, 0, EXPECT_STATUS_ADM(0, 0, 1), 1, 0, 0, 18, 11, 0, 7 },
		{ 0 },
	};
	diff_expects *expected;
	int i, j;

	g_repo = cl_git_sandbox_init("attr");

	cl_assert((a = resolve_commit_oid_to_tree(g_repo, a_commit)) != NULL);
	cl_assert((b = resolve_commit_oid_to_tree(g_repo, b_commit)) != NULL);
	cl_assert((c = resolve_commit_oid_to_tree(g_repo, c_commit)) != NULL);
	cl_assert((d = resolve_commit_oid_to_tree(g_repo, d_commit)) != NULL);

	for (i = 0; test_expects[i].files > 0; i++) {
		memset(&actual, 0, sizeof(actual)); /* clear accumulator */
		opts = test_options[i];

		if (test_ab_or_cd[i] == 0)
			cl_git_pass(git_diff_tree_to_tree(&diff, g_repo, a, b, &opts));
		else
			cl_git_pass(git_diff_tree_to_tree(&diff, g_repo, c, d, &opts));

		cl_git_pass(git_diff_foreach(
			diff, diff_file_cb, diff_binary_cb, diff_hunk_cb, diff_line_cb, &actual));

		expected = &test_expects[i];
		cl_assert_equal_i(actual.files,     expected->files);
		for (j = GIT_DELTA_UNMODIFIED; j <= GIT_DELTA_TYPECHANGE; ++j)
			cl_assert_equal_i(expected->file_status[j], actual.file_status[j]);
		cl_assert_equal_i(actual.hunks,     expected->hunks);
		cl_assert_equal_i(actual.lines,     expected->lines);
		cl_assert_equal_i(actual.line_ctxt, expected->line_ctxt);
		cl_assert_equal_i(actual.line_adds, expected->line_adds);
		cl_assert_equal_i(actual.line_dels, expected->line_dels);

		git_diff_free(diff);
		diff = NULL;
	}

	git_tree_free(c);
	git_tree_free(d);
}

void test_diff_tree__bare(void)
{
	const char *a_commit = "8496071c1b46c85";
	const char *b_commit = "be3563ae3f79";

	g_repo = cl_git_sandbox_init("testrepo.git");

	cl_assert((a = resolve_commit_oid_to_tree(g_repo, a_commit)) != NULL);
	cl_assert((b = resolve_commit_oid_to_tree(g_repo, b_commit)) != NULL);

	opts.context_lines = 1;
	opts.interhunk_lines = 1;

	cl_git_pass(git_diff_tree_to_tree(&diff, g_repo, a, b, &opts));

	cl_git_pass(git_diff_foreach(
		diff, diff_file_cb, diff_binary_cb, diff_hunk_cb, diff_line_cb, &expect));

	cl_assert_equal_i(3, expect.files);
	cl_assert_equal_i(2, expect.file_status[GIT_DELTA_ADDED]);
	cl_assert_equal_i(0, expect.file_status[GIT_DELTA_DELETED]);
	cl_assert_equal_i(1, expect.file_status[GIT_DELTA_MODIFIED]);

	cl_assert_equal_i(3, expect.hunks);

	cl_assert_equal_i(4, expect.lines);
	cl_assert_equal_i(0, expect.line_ctxt);
	cl_assert_equal_i(3, expect.line_adds);
	cl_assert_equal_i(1, expect.line_dels);
}

void test_diff_tree__merge(void)
{
	/* grabbed a couple of commit oids from the history of the attr repo */
	const char *a_commit = "605812a";
	const char *b_commit = "370fe9ec22";
	const char *c_commit = "f5b0af1fb4f5c";
	git_tree *c;
	git_diff *diff1 = NULL, *diff2 = NULL;

	g_repo = cl_git_sandbox_init("attr");

	cl_assert((a = resolve_commit_oid_to_tree(g_repo, a_commit)) != NULL);
	cl_assert((b = resolve_commit_oid_to_tree(g_repo, b_commit)) != NULL);
	cl_assert((c = resolve_commit_oid_to_tree(g_repo, c_commit)) != NULL);

	cl_git_pass(git_diff_tree_to_tree(&diff1, g_repo, a, b, NULL));

	cl_git_pass(git_diff_tree_to_tree(&diff2, g_repo, c, b, NULL));

	git_tree_free(c);

	cl_git_pass(git_diff_merge(diff1, diff2));

	git_diff_free(diff2);

	cl_git_pass(git_diff_foreach(
		diff1, diff_file_cb, diff_binary_cb, diff_hunk_cb, diff_line_cb, &expect));

	cl_assert_equal_i(6, expect.files);
	cl_assert_equal_i(2, expect.file_status[GIT_DELTA_ADDED]);
	cl_assert_equal_i(1, expect.file_status[GIT_DELTA_DELETED]);
	cl_assert_equal_i(3, expect.file_status[GIT_DELTA_MODIFIED]);

	cl_assert_equal_i(6, expect.hunks);

	cl_assert_equal_i(59, expect.lines);
	cl_assert_equal_i(1, expect.line_ctxt);
	cl_assert_equal_i(36, expect.line_adds);
	cl_assert_equal_i(22, expect.line_dels);

	git_diff_free(diff1);
}

void test_diff_tree__larger_hunks(void)
{
	const char *a_commit = "d70d245ed97ed2aa596dd1af6536e4bfdb047b69";
	const char *b_commit = "7a9e0b02e63179929fed24f0a3e0f19168114d10";
	size_t d, num_d, h, num_h, l, num_l;
	git_patch *patch;
	const git_diff_hunk *hunk;
	const git_diff_line *line;

	g_repo = cl_git_sandbox_init("diff");

	cl_assert((a = resolve_commit_oid_to_tree(g_repo, a_commit)) != NULL);
	cl_assert((b = resolve_commit_oid_to_tree(g_repo, b_commit)) != NULL);

	opts.context_lines = 1;
	opts.interhunk_lines = 0;

	cl_git_pass(git_diff_tree_to_tree(&diff, g_repo, a, b, &opts));

	num_d = git_diff_num_deltas(diff);
	for (d = 0; d < num_d; ++d) {
		cl_git_pass(git_patch_from_diff(&patch, diff, d));
		cl_assert(patch);

		num_h = git_patch_num_hunks(patch);
		for (h = 0; h < num_h; h++) {
			cl_git_pass(git_patch_get_hunk(&hunk, &num_l, patch, h));

			for (l = 0; l < num_l; ++l) {
				cl_git_pass(git_patch_get_line_in_hunk(&line, patch, h, l));
				cl_assert(line);
			}

			cl_git_fail(git_patch_get_line_in_hunk(&line, patch, h, num_l));
		}

		cl_git_fail(git_patch_get_hunk(&hunk, &num_l, patch, num_h));

		git_patch_free(patch);
	}

	cl_git_fail(git_patch_from_diff(&patch, diff, num_d));

	cl_assert_equal_i(2, (int)num_d);
}

void test_diff_tree__checks_options_version(void)
{
	const char *a_commit = "8496071c1b46c85";
	const char *b_commit = "be3563ae3f79";
	const git_error *err;

	g_repo = cl_git_sandbox_init("testrepo.git");

	cl_assert((a = resolve_commit_oid_to_tree(g_repo, a_commit)) != NULL);
	cl_assert((b = resolve_commit_oid_to_tree(g_repo, b_commit)) != NULL);

	opts.version = 0;
	cl_git_fail(git_diff_tree_to_tree(&diff, g_repo, a, b, &opts));
	err = git_error_last();
	cl_assert_equal_i(GIT_ERROR_INVALID, err->klass);

	git_error_clear();
	opts.version = 1024;
	cl_git_fail(git_diff_tree_to_tree(&diff, g_repo, a, b, &opts));
	err = git_error_last();
}

static void process_tree_to_tree_diffing(
	const char *old_commit,
	const char *new_commit)
{
	g_repo = cl_git_sandbox_init("unsymlinked.git");

	cl_assert((a = resolve_commit_oid_to_tree(g_repo, old_commit)) != NULL);
	cl_assert((b = resolve_commit_oid_to_tree(g_repo, new_commit)) != NULL);

	cl_git_pass(git_diff_tree_to_tree(&diff, g_repo, a, b, &opts));

	cl_git_pass(git_diff_foreach(
		diff, diff_file_cb, NULL, NULL, NULL, &expect));
}

void test_diff_tree__symlink_blob_mode_changed_to_regular_file(void)
{
	/*
	* $ git diff  7fccd7..806999
	* diff --git a/include/Nu/Nu.h b/include/Nu/Nu.h
	* deleted file mode 120000
	* index 19bf568..0000000
	* --- a/include/Nu/Nu.h
	* +++ /dev/null
	* @@ -1 +0,0 @@
	* -../../objc/Nu.h
	* \ No newline at end of file
	* diff --git a/include/Nu/Nu.h b/include/Nu/Nu.h
	* new file mode 100644
	* index 0000000..f9e6561
	* --- /dev/null
	* +++ b/include/Nu/Nu.h
	* @@ -0,0 +1 @@
	* +awesome content
	* diff --git a/objc/Nu.h b/objc/Nu.h
	* deleted file mode 100644
	* index f9e6561..0000000
	* --- a/objc/Nu.h
	* +++ /dev/null
	* @@ -1 +0,0 @@
	* -awesome content
	*/

	process_tree_to_tree_diffing("7fccd7", "806999");

	cl_assert_equal_i(3, expect.files);
	cl_assert_equal_i(2, expect.file_status[GIT_DELTA_DELETED]);
	cl_assert_equal_i(0, expect.file_status[GIT_DELTA_MODIFIED]);
	cl_assert_equal_i(1, expect.file_status[GIT_DELTA_ADDED]);
	cl_assert_equal_i(0, expect.file_status[GIT_DELTA_TYPECHANGE]);
}

void test_diff_tree__symlink_blob_mode_changed_to_regular_file_as_typechange(void)
{
	/*
	 * $ git diff  7fccd7..a8595c
	 * diff --git a/include/Nu/Nu.h b/include/Nu/Nu.h
	 * deleted file mode 120000
	 * index 19bf568..0000000
	 * --- a/include/Nu/Nu.h
	 * +++ /dev/null
	 * @@ -1 +0,0 @@
	 * -../../objc/Nu.h
	 * \ No newline at end of file
	 * diff --git a/include/Nu/Nu.h b/include/Nu/Nu.h
	 * new file mode 100755
	 * index 0000000..f9e6561
	 * --- /dev/null
	 * +++ b/include/Nu/Nu.h
	 * @@ -0,0 +1 @@
	 * +awesome content
	 * diff --git a/objc/Nu.h b/objc/Nu.h
	 * deleted file mode 100644
	 * index f9e6561..0000000
	 * --- a/objc/Nu.h
	 * +++ /dev/null
	 * @@ -1 +0,0 @@
	 * -awesome content
	*/

	opts.flags = GIT_DIFF_INCLUDE_TYPECHANGE;
	process_tree_to_tree_diffing("7fccd7", "a8595c");

	cl_assert_equal_i(2, expect.files);
	cl_assert_equal_i(1, expect.file_status[GIT_DELTA_DELETED]);
	cl_assert_equal_i(0, expect.file_status[GIT_DELTA_MODIFIED]);
	cl_assert_equal_i(0, expect.file_status[GIT_DELTA_ADDED]);
	cl_assert_equal_i(1, expect.file_status[GIT_DELTA_TYPECHANGE]);
}

void test_diff_tree__regular_blob_mode_changed_to_executable_file(void)
{
	/*
	 * $ git diff 806999..a8595c
	 * diff --git a/include/Nu/Nu.h b/include/Nu/Nu.h
	 * old mode 100644
	 * new mode 100755
	 */

	process_tree_to_tree_diffing("806999", "a8595c");

	cl_assert_equal_i(1, expect.files);
	cl_assert_equal_i(0, expect.file_status[GIT_DELTA_DELETED]);
	cl_assert_equal_i(1, expect.file_status[GIT_DELTA_MODIFIED]);
	cl_assert_equal_i(0, expect.file_status[GIT_DELTA_ADDED]);
	cl_assert_equal_i(0, expect.file_status[GIT_DELTA_TYPECHANGE]);
}

void test_diff_tree__issue_1397(void)
{
	/* this test shows that it is not needed */

	g_repo = cl_git_sandbox_init("issue_1397");

	cl_repo_set_bool(g_repo, "core.autocrlf", true);

	cl_assert((a = resolve_commit_oid_to_tree(g_repo, "8a7ef04")) != NULL);
	cl_assert((b = resolve_commit_oid_to_tree(g_repo, "7f483a7")) != NULL);

	cl_git_pass(git_diff_tree_to_tree(&diff, g_repo, a, b, &opts));

	cl_git_pass(git_diff_foreach(diff,
		diff_file_cb, diff_binary_cb, diff_hunk_cb, diff_line_cb, &expect));

	cl_assert_equal_i(1, expect.files);
	cl_assert_equal_i(0, expect.file_status[GIT_DELTA_DELETED]);
	cl_assert_equal_i(1, expect.file_status[GIT_DELTA_MODIFIED]);
	cl_assert_equal_i(0, expect.file_status[GIT_DELTA_ADDED]);
	cl_assert_equal_i(0, expect.file_status[GIT_DELTA_TYPECHANGE]);
}

static void set_config_int(git_repository *repo, const char *name, int value)
{
	git_config *cfg;

	cl_git_pass(git_repository_config(&cfg, repo));
	cl_git_pass(git_config_set_int32(cfg, name, value));
	git_config_free(cfg);
}

void test_diff_tree__diff_configs(void)
{
	const char *a_commit = "d70d245e";
	const char *b_commit = "7a9e0b02";

	g_repo = cl_git_sandbox_init("diff");

	cl_assert((a = resolve_commit_oid_to_tree(g_repo, a_commit)) != NULL);
	cl_assert((b = resolve_commit_oid_to_tree(g_repo, b_commit)) != NULL);

	cl_git_pass(git_diff_tree_to_tree(&diff, g_repo, a, b, NULL));

	cl_git_pass(git_diff_foreach(diff,
		diff_file_cb, diff_binary_cb, diff_hunk_cb, diff_line_cb, &expect));

	cl_assert_equal_i(2, expect.files);
	cl_assert_equal_i(2, expect.file_status[GIT_DELTA_MODIFIED]);
	cl_assert_equal_i(6, expect.hunks);
	cl_assert_equal_i(55, expect.lines);
	cl_assert_equal_i(33, expect.line_ctxt);
	cl_assert_equal_i(7, expect.line_adds);
	cl_assert_equal_i(15, expect.line_dels);

	git_diff_free(diff);
	diff = NULL;

	set_config_int(g_repo, "diff.context", 1);

	memset(&expect, 0, sizeof(expect));

	cl_git_pass(git_diff_tree_to_tree(&diff, g_repo, a, b, NULL));

	cl_git_pass(git_diff_foreach(diff,
		diff_file_cb, diff_binary_cb, diff_hunk_cb, diff_line_cb, &expect));

	cl_assert_equal_i(2, expect.files);
	cl_assert_equal_i(2, expect.file_status[GIT_DELTA_MODIFIED]);
	cl_assert_equal_i(7, expect.hunks);
	cl_assert_equal_i(34, expect.lines);
	cl_assert_equal_i(12, expect.line_ctxt);
	cl_assert_equal_i(7, expect.line_adds);
	cl_assert_equal_i(15, expect.line_dels);

	git_diff_free(diff);
	diff = NULL;

	set_config_int(g_repo, "diff.context", 0);
	set_config_int(g_repo, "diff.noprefix", 1);

	memset(&expect, 0, sizeof(expect));

	cl_git_pass(git_diff_tree_to_tree(&diff, g_repo, a, b, NULL));

	cl_git_pass(git_diff_foreach(diff,
		diff_file_cb, diff_binary_cb, diff_hunk_cb, diff_line_cb, &expect));

	cl_assert_equal_i(2, expect.files);
	cl_assert_equal_i(2, expect.file_status[GIT_DELTA_MODIFIED]);
	cl_assert_equal_i(7, expect.hunks);
	cl_assert_equal_i(22, expect.lines);
	cl_assert_equal_i(0, expect.line_ctxt);
	cl_assert_equal_i(7, expect.line_adds);
	cl_assert_equal_i(15, expect.line_dels);
}

void test_diff_tree__diff_tree_with_empty_dir_entry_succeeds(void)
{
	const char *content = "This is a blob\n";
	const git_diff_delta *delta;
	git_oid empty_tree, invalid_tree, blob;
	git_buf patch = GIT_BUF_INIT;
	git_treebuilder *builder;

	g_repo = cl_git_sandbox_init("empty_standard_repo");

	cl_git_pass(git_blob_create_from_buffer(&blob, g_repo, content, strlen(content)));
	cl_git_pass(git_treebuilder_new(&builder, g_repo, NULL));
	cl_git_pass(git_treebuilder_write(&empty_tree, builder));
	cl_git_pass(git_treebuilder_insert(NULL, builder, "empty_tree", &empty_tree, GIT_FILEMODE_TREE));
	cl_git_pass(git_treebuilder_insert(NULL, builder, "blob", &blob, GIT_FILEMODE_BLOB));
	cl_git_pass(git_treebuilder_write(&invalid_tree, builder));

	cl_git_pass(git_tree_lookup(&a, g_repo, &empty_tree));
	cl_git_pass(git_tree_lookup(&b, g_repo, &invalid_tree));
	cl_git_pass(git_diff_tree_to_tree(&diff, g_repo, a, b, NULL));

	cl_git_pass(git_diff_foreach(diff,
		diff_file_cb, diff_binary_cb, diff_hunk_cb, diff_line_cb, &expect));
	cl_assert_equal_i(1, expect.files);
	cl_assert_equal_i(0, expect.file_status[GIT_DELTA_MODIFIED]);
	cl_assert_equal_i(1, expect.hunks);
	cl_assert_equal_i(1, expect.lines);
	cl_assert_equal_i(0, expect.line_ctxt);
	cl_assert_equal_i(1, expect.line_adds);
	cl_assert_equal_i(0, expect.line_dels);

	cl_git_pass(git_diff_to_buf(&patch, diff, GIT_DIFF_FORMAT_PATCH));
	cl_assert_equal_s(patch.ptr,
		"diff --git a/blob b/blob\n"
		"new file mode 100644\n"
		"index 0000000..bbf2e80\n"
		"--- /dev/null\n"
		"+++ b/blob\n"
		"@@ -0,0 +1 @@\n"
		"+This is a blob\n");

	cl_assert_equal_i(git_diff_num_deltas(diff), 1);
	delta = git_diff_get_delta(diff, 0);
	cl_assert_equal_s(delta->new_file.path, "blob");

	git_treebuilder_free(builder);
	git_buf_dispose(&patch);
}

static git_tree *create_tree(const char **paths_and_contents)
{
	git_tree_update updates[8];
	git_tree *tree;
	git_oid id;
	size_t i;

	for (i = 0; paths_and_contents[i * 2]; i++) {
		const char *content = paths_and_contents[i * 2 + 1];

		cl_assert(i < ARRAY_SIZE(updates));

		memset(&updates[i], 0, sizeof(git_tree_update));
		updates[i].action = GIT_TREE_UPDATE_UPSERT;
		updates[i].filemode = GIT_FILEMODE_BLOB;
		updates[i].path = paths_and_contents[i * 2];
		cl_git_pass(git_blob_create_from_buffer(&updates[i].id,
			g_repo, content, strlen(content)));
	}

	cl_git_pass(git_tree_create_updated(&id, g_repo, NULL, i, updates));
	cl_git_pass(git_tree_lookup(&tree, g_repo, &id));

	return tree;
}

/* entries must be added in git order */
static void add_raw_tree_entry(
	git_str *raw, const char *mode, const char *name, const git_oid *id)
{
	git_str_printf(raw, "%s %s", mode, name);
	git_str_putc(raw, '\0');
	git_str_put(raw, (const char *)id->id, GIT_OID_SHA1_SIZE);
	cl_assert(!git_str_oom(raw));
}

static git_tree *write_raw_tree(git_str *raw)
{
	git_odb *odb;
	git_tree *tree;
	git_oid id;

	cl_git_pass(git_repository_odb(&odb, g_repo));
	cl_git_pass(git_odb_write(&id, odb, raw->ptr, raw->size, GIT_OBJECT_TREE));
	cl_git_pass(git_tree_lookup(&tree, g_repo, &id));

	git_odb_free(odb);
	git_str_clear(raw);

	return tree;
}

void test_diff_tree__does_not_load_unmodified_subtrees(void)
{
	git_str raw = GIT_STR_INIT;
	git_oid missing, one, two;

	g_repo = cl_git_sandbox_init("empty_standard_repo");

	cl_git_pass(git_blob_create_from_buffer(&one, g_repo, "one\n", 4));
	cl_git_pass(git_blob_create_from_buffer(&two, g_repo, "two\n", 4));

	/* both trees point to the same subtree, which is not in the odb */
	git_oid_cpy(&missing, &one);
	missing.id[0] ^= 0xff;

	add_raw_tree_entry(&raw, "100644", "file", &one);
	add_raw_tree_entry(&raw, "40000", "missing", &missing);
	a = write_raw_tree(&raw);

	add_raw_tree_entry(&raw, "100644", "file", &two);
	add_raw_tree_entry(&raw, "40000", "missing", &missing);
	b = write_raw_tree(&raw);

	cl_git_pass(git_diff_tree_to_tree(&diff, g_repo, a, b, NULL));

	cl_assert_equal_i(1, git_diff_num_deltas(diff));
	cl_assert_equal_i(GIT_DELTA_MODIFIED, git_diff_get_delta(diff, 0)->status);
	cl_assert_equal_s("file", git_diff_get_delta(diff, 0)->new_file.path);

	git_diff_free(diff);
	diff = NULL;

	/* unmodified entries can only be listed by reading the subtree */
	opts.flags = GIT_DIFF_INCLUDE_UNMODIFIED;
	cl_git_fail(git_diff_tree_to_tree(&diff, g_repo, a, b, &opts));

	git_str_dispose(&raw);
}

static int notify_only_files_cb(
	const git_diff *diff_so_far,
	const git_diff_delta *delta,
	const char *matched_pathspec,
	void *payload)
{
	size_t *calls = payload;

	GIT_UNUSED(diff_so_far);
	GIT_UNUSED(matched_pathspec);

	cl_assert(!S_ISDIR(delta->old_file.mode));
	cl_assert(!S_ISDIR(delta->new_file.mode));

	(*calls)++;
	return 0;
}

void test_diff_tree__notify_cb_is_not_called_for_trees(void)
{
	const char *old_paths[] = {
		"dir/a", "one\n", "dir/b", "two\n", "same/c", "three\n", NULL
	};
	const char *new_paths[] = {
		"dir/a", "uno\n", "dir/b", "dos\n", "same/c", "three\n", NULL
	};
	size_t calls = 0;

	g_repo = cl_git_sandbox_init("empty_standard_repo");

	a = create_tree(old_paths);
	b = create_tree(new_paths);

	opts.notify_cb = notify_only_files_cb;
	opts.payload = &calls;

	cl_git_pass(git_diff_tree_to_tree(&diff, g_repo, a, b, &opts));

	cl_assert_equal_i(2, calls);
	cl_assert_equal_i(2, git_diff_num_deltas(diff));
	cl_assert_equal_s("dir/a", git_diff_get_delta(diff, 0)->new_file.path);
	cl_assert_equal_s("dir/b", git_diff_get_delta(diff, 1)->new_file.path);
}

void test_diff_tree__ignore_case_with_trees_that_differ_in_case(void)
{
	const char *old_paths[] = {
		"FOO/a", "same\n", "foo/b", "old\n", NULL
	};
	const char *new_paths[] = {
		"FOO/a", "same\n", "foo/b", "new\n", NULL
	};

	g_repo = cl_git_sandbox_init("empty_standard_repo");

	a = create_tree(old_paths);
	b = create_tree(new_paths);

	opts.flags = GIT_DIFF_IGNORE_CASE;
	cl_git_pass(git_diff_tree_to_tree(&diff, g_repo, a, b, &opts));

	cl_assert_equal_i(1, git_diff_num_deltas(diff));
	cl_assert_equal_i(GIT_DELTA_MODIFIED, git_diff_get_delta(diff, 0)->status);
	cl_assert_equal_s("foo/b", git_diff_get_delta(diff, 0)->new_file.path);
}

void test_diff_tree__ignore_case_with_casechange_of_tree(void)
{
	const char *old_paths[] = { "Dir/a", "content\n", NULL };
	const char *new_paths[] = { "dir/a", "content\n", NULL };
	const git_diff_delta *delta;
	size_t i;

	g_repo = cl_git_sandbox_init("empty_standard_repo");

	a = create_tree(old_paths);
	b = create_tree(new_paths);

	opts.flags = GIT_DIFF_IGNORE_CASE | GIT_DIFF_INCLUDE_CASECHANGE;
	cl_git_pass(git_diff_tree_to_tree(&diff, g_repo, a, b, &opts));

	cl_assert_equal_i(2, git_diff_num_deltas(diff));

	for (i = 0; i < git_diff_num_deltas(diff); i++) {
		delta = git_diff_get_delta(diff, i);

		if (delta->status == GIT_DELTA_DELETED)
			cl_assert_equal_s("Dir/a", delta->old_file.path);
		else if (delta->status == GIT_DELTA_ADDED)
			cl_assert_equal_s("dir/a", delta->new_file.path);
		else
			cl_fail("unexpected delta status");
	}
}

void test_diff_tree__tree_with_nonstandard_mode(void)
{
	const char *old_paths[] = { "file", "one\n", NULL };
	const char *new_paths[] = { "file", "two\n", NULL };
	git_str raw = GIT_STR_INIT;
	git_tree *old_dir, *new_dir;

	g_repo = cl_git_sandbox_init("empty_standard_repo");

	old_dir = create_tree(old_paths);
	new_dir = create_tree(new_paths);

	add_raw_tree_entry(&raw, "40755", "dir", git_tree_id(old_dir));
	a = write_raw_tree(&raw);

	add_raw_tree_entry(&raw, "40755", "dir", git_tree_id(new_dir));
	b = write_raw_tree(&raw);

	cl_git_pass(git_diff_tree_to_tree(&diff, g_repo, a, b, &opts));

	cl_assert_equal_i(1, git_diff_num_deltas(diff));
	cl_assert_equal_i(GIT_DELTA_MODIFIED, git_diff_get_delta(diff, 0)->status);
	cl_assert_equal_s("dir/file", git_diff_get_delta(diff, 0)->new_file.path);

	git_tree_free(old_dir);
	git_tree_free(new_dir);
	git_str_dispose(&raw);
}

void test_diff_tree__typechange_trees_with_empty_tree_in_between(void)
{
	const char *subdir_paths[] = { "bar", "bar\n", NULL };
	git_str raw = GIT_STR_INIT;
	git_tree *empty, *subdir;
	git_oid blob;

	g_repo = cl_git_sandbox_init("empty_standard_repo");

	empty = write_raw_tree(&raw);
	subdir = create_tree(subdir_paths);
	cl_git_pass(git_blob_create_from_buffer(&blob, g_repo, "foo\n", 4));

	/* `foo.txt/` sorts between `foo` and `foo/` */
	add_raw_tree_entry(&raw, "100644", "foo", &blob);
	add_raw_tree_entry(&raw, "40000", "foo.txt", git_tree_id(empty));
	a = write_raw_tree(&raw);

	add_raw_tree_entry(&raw, "40000", "foo.txt", git_tree_id(empty));
	add_raw_tree_entry(&raw, "40000", "foo", git_tree_id(subdir));
	b = write_raw_tree(&raw);

	opts.flags = GIT_DIFF_INCLUDE_TYPECHANGE_TREES;
	cl_git_pass(git_diff_tree_to_tree(&diff, g_repo, a, b, &opts));

	cl_assert_equal_i(2, git_diff_num_deltas(diff));
	cl_assert_equal_i(GIT_DELTA_TYPECHANGE, git_diff_get_delta(diff, 0)->status);
	cl_assert_equal_s("foo", git_diff_get_delta(diff, 0)->old_file.path);
	cl_assert_equal_i(GIT_DELTA_ADDED, git_diff_get_delta(diff, 1)->status);
	cl_assert_equal_s("foo/bar", git_diff_get_delta(diff, 1)->new_file.path);

	git_tree_free(empty);
	git_tree_free(subdir);
	git_str_dispose(&raw);
}
