

#include "clar_libgit2.h"
#include "csvtoyaml.h"
#include "testdata.h"
#include "../diff/diff_helpers.h"
#include <stdio.h>

static git_repository *g_repo = NULL;
static git_textconv* yaml_filter = NULL;

void test_textconv_diff__initialize(void)
{
	yaml_filter = create_csv_to_yaml_textconv();
	cl_git_pass(git_textconv_register("csv2yaml", yaml_filter));
	g_repo = cl_git_sandbox_init("textconv");
}

void test_textconv_diff__cleanup(void)
{
	cl_git_sandbox_cleanup();
	g_repo = NULL;
	git_textconv_unregister("csv2yaml");
	git__free(yaml_filter);
}

void test_textconv_diff__versions_default(void)
{
	git_buf out = GIT_BUF_INIT;
	git_diff* diff;
	git_diff_options opts = GIT_DIFF_OPTIONS_INIT;
	diff_expects results;
	const char *a_commit = "4bcc371555";
	const char *b_commit = "f7299e9f74";
	git_tree *a, *b;

	memset(&results, 0, sizeof(results));

	cl_assert((a = resolve_commit_oid_to_tree(g_repo, a_commit)) != NULL);
	cl_assert((b = resolve_commit_oid_to_tree(g_repo, b_commit)) != NULL);

	opts.context_lines = 1;
	opts.interhunk_lines = 1;

	cl_git_pass(git_diff_tree_to_tree(&diff, g_repo, a, b, &opts));

	cl_git_pass(git_diff_foreach(
		diff, diff_file_cb, diff_binary_cb, diff_hunk_cb, diff_line_cb, &results));

	cl_assert_equal_i(3, results.files);
	cl_assert_equal_i(0, results.file_status[GIT_DELTA_ADDED]);
	cl_assert_equal_i(0, results.file_status[GIT_DELTA_DELETED]);
	cl_assert_equal_i(3, results.file_status[GIT_DELTA_MODIFIED]);

	cl_assert_equal_i(3, results.hunks);

	cl_assert_equal_i(12, results.lines); // 4 lines per change?
	cl_assert_equal_i(6, results.line_ctxt);
	cl_assert_equal_i(3, results.line_adds);
	cl_assert_equal_i(3, results.line_dels);

	git_diff_free(diff);
	git_tree_free(a);
	git_tree_free(b);
	git_buf_free(&out);
}

void test_textconv_diff__tree_to_workdir_default(void)
{
	git_buf out = GIT_BUF_INIT;
	git_diff* diff;
	git_diff_options opts = GIT_DIFF_OPTIONS_INIT;
	diff_expects results;
	const char *b_commit = "f7299e9f74";
	git_tree *b;

	memset(&results, 0, sizeof(results));

	cl_assert((b = resolve_commit_oid_to_tree(g_repo, b_commit)) != NULL);

	opts.context_lines = 1;
	opts.interhunk_lines = 1;

	cl_git_pass(git_diff_tree_to_workdir_with_index(&diff, g_repo, b, &opts));

	cl_git_pass(git_diff_foreach(
		diff, diff_file_cb, diff_binary_cb, diff_hunk_cb, diff_line_cb, &results));

	cl_assert_equal_i(1, results.files);
	cl_assert_equal_i(0, results.file_status[GIT_DELTA_ADDED]);
	cl_assert_equal_i(0, results.file_status[GIT_DELTA_DELETED]);
	cl_assert_equal_i(1, results.file_status[GIT_DELTA_MODIFIED]);

	cl_assert_equal_i(1, results.hunks);

	cl_assert_equal_i(3, results.lines);
	cl_assert_equal_i(1, results.line_ctxt);
	cl_assert_equal_i(1, results.line_adds);
	cl_assert_equal_i(1, results.line_dels);

	git_diff_free(diff);
	git_tree_free(b);
	git_buf_free(&out);
}


void test_textconv_diff__versions_textconv_on(void)
{
	git_buf out = GIT_BUF_INIT;
	git_diff* diff;
	git_diff_options opts = GIT_DIFF_OPTIONS_INIT;
	diff_expects results;
	const char *a_commit = "4bcc371555";
	const char *b_commit = "f7299e9f74";
	git_tree *a, *b;

	memset(&results, 0, sizeof(results));

	cl_assert((a = resolve_commit_oid_to_tree(g_repo, a_commit)) != NULL);
	cl_assert((b = resolve_commit_oid_to_tree(g_repo, b_commit)) != NULL);

	opts.context_lines = 1;
	opts.interhunk_lines = 1;
	opts.flags |= GIT_DIFF_ENABLE_TEXTCONV; // <--- switch it on

	cl_git_pass(git_diff_tree_to_tree(&diff, g_repo, a, b, &opts));

	cl_git_pass(git_diff_foreach(
		diff, diff_file_cb, diff_binary_cb, diff_hunk_cb, diff_line_cb, &results));

	cl_assert_equal_i(3, results.files);
	cl_assert_equal_i(0, results.file_status[GIT_DELTA_ADDED]);
	cl_assert_equal_i(0, results.file_status[GIT_DELTA_DELETED]);
	cl_assert_equal_i(3, results.file_status[GIT_DELTA_MODIFIED]);

	cl_assert_equal_i(3, results.hunks);

	cl_assert_equal_i(18, results.lines); // 4 lines per change?
	cl_assert_equal_i(8, results.line_ctxt);
	cl_assert_equal_i(5, results.line_adds);
	cl_assert_equal_i(5, results.line_dels);

	git_diff_free(diff);
	git_tree_free(a);
	git_tree_free(b);
	git_buf_free(&out);
}

void test_textconv_diff__tree_to_workdir_textconv_on(void)
{
	git_buf out = GIT_BUF_INIT;
	git_diff* diff;
	git_diff_options opts = GIT_DIFF_OPTIONS_INIT;
	diff_expects results;
	const char *b_commit = "f7299e9f74";
	git_tree *b;

	memset(&results, 0, sizeof(results));

	cl_assert((b = resolve_commit_oid_to_tree(g_repo, b_commit)) != NULL);

	opts.context_lines = 1;
	opts.interhunk_lines = 1;
	opts.flags |= GIT_DIFF_ENABLE_TEXTCONV; // <--- switch it on

	cl_git_pass(git_diff_tree_to_workdir_with_index(&diff, g_repo, b, &opts));

	cl_git_pass(git_diff_foreach(
								 diff, diff_file_cb, diff_binary_cb, diff_hunk_cb, diff_line_cb, &results));

	cl_assert_equal_i(1, results.files);
	cl_assert_equal_i(0, results.file_status[GIT_DELTA_ADDED]);
	cl_assert_equal_i(0, results.file_status[GIT_DELTA_DELETED]);
	cl_assert_equal_i(1, results.file_status[GIT_DELTA_MODIFIED]);

	cl_assert_equal_i(1, results.hunks);

	cl_assert_equal_i(7, results.lines);
	cl_assert_equal_i(3, results.line_ctxt);
	cl_assert_equal_i(2, results.line_adds);
	cl_assert_equal_i(2, results.line_dels);

	git_diff_free(diff);
	git_tree_free(b);
	git_buf_free(&out);
}


