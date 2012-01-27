#include "clay_libgit2.h"
#include "fileops.h"
#include "git2/diff.h"

static git_repository *g_repo = NULL;

void test_diff_blob__initialize(void)
{
	cl_fixture_sandbox("attr");
	cl_git_pass(p_rename("attr/.gitted", "attr/.git"));
	cl_git_pass(p_rename("attr/gitattributes", "attr/.gitattributes"));
	cl_git_pass(git_repository_open(&g_repo, "attr/.git"));
}

void test_diff_blob__cleanup(void)
{
	git_repository_free(g_repo);
	g_repo = NULL;
	cl_fixture_cleanup("attr");
}

typedef struct {
	int files;
	int hunks;
	int hunk_new_lines;
	int hunk_old_lines;
	int lines;
	int line_ctxt;
	int line_adds;
	int line_dels;
} diff_expects;

static void log(const char *str, int n)
{
	FILE *fp = fopen("/Users/rb/tmp/diff.log", "a");
	if (n > 0)
		fprintf(fp, "%.*s", n, str);
	else
		fputs(str, fp);
	fclose(fp);
}

static int diff_file_fn(
	void *cb_data,
	const git_oid *old,
	const char *old_path,
	int old_mode,
	const git_oid *new,
	const char *new_path,
	int new_mode)
{
	diff_expects *e = cb_data;
	e->files++;
	log("-- file --\n", 0);
	return 0;
}

static int diff_hunk_fn(
	void *cb_data,
	int old_start,
	int old_lines,
	int new_start,
	int new_lines)
{
	diff_expects *e = cb_data;
	e->hunks++;
	e->hunk_old_lines += old_lines;
	e->hunk_new_lines += new_lines;
	log("-- hunk --\n", 0);
	return 0;
}

static int diff_line_fn(
	void *cb_data,
	int origin,
	const char *content,
	size_t content_len)
{
	diff_expects *e = cb_data;
	e->lines++;
	switch (origin) {
	case GIT_DIFF_LINE_CONTEXT:
		log("[ ]", 3);
		e->line_ctxt++;
		break;
	case GIT_DIFF_LINE_ADDITION:
		log("[+]", 3);
		e->line_adds++;
		break;
	case GIT_DIFF_LINE_DELETION:
		log("[-]", 3);
		e->line_dels++;
		break;
	default:
		cl_assert("Unknown diff line origin" == 0);
	}
	log(content, content_len);
	return 0;
}

void test_diff_blob__0(void)
{
	int err;
	git_blob *a, *b, *c, *d;
	git_oid a_oid, b_oid, c_oid, d_oid;
	git_diff_opts opts;
	diff_expects exp;

	/* tests/resources/attr/root_test1 */
	cl_git_pass(git_oid_fromstrn(&a_oid, "45141a79", 8));
	cl_git_pass(git_blob_lookup_prefix(&a, g_repo, &a_oid, 4));

	/* tests/resources/attr/root_test2 */
	cl_git_pass(git_oid_fromstrn(&b_oid, "4d713dc4", 8));
	cl_git_pass(git_blob_lookup_prefix(&b, g_repo, &b_oid, 4));

	/* tests/resources/attr/root_test3 */
	cl_git_pass(git_oid_fromstrn(&c_oid, "c96bbb2c2557a832", 16));
	cl_git_pass(git_blob_lookup_prefix(&c, g_repo, &c_oid, 8));

	/* tests/resources/attr/root_test4.txt */
	cl_git_pass(git_oid_fromstrn(&d_oid, "fe773770c5a6", 12));
	cl_git_pass(git_blob_lookup_prefix(&d, g_repo, &d_oid, 6));

	/* Doing the equivalent of a `diff -U 2` on these files */

	opts.context_lines = 2;
	opts.interhunk_lines = 0;
	opts.ignore_whitespace = 0;
	opts.file_cb = diff_file_fn;
	opts.hunk_cb = diff_hunk_fn;
	opts.line_cb = diff_line_fn;
	opts.cb_data = &exp;

	memset(&exp, 0, sizeof(exp));
	cl_git_pass(git_diff_blobs(g_repo, a, b, &opts));

	cl_assert(exp.files == 1);
	cl_assert(exp.hunks == 1);
	cl_assert(exp.lines == 6);
	cl_assert(exp.line_ctxt == 1);
	cl_assert(exp.line_adds == 5);
	cl_assert(exp.line_dels == 0);

	memset(&exp, 0, sizeof(exp));
	cl_git_pass(git_diff_blobs(g_repo, b, c, &opts));

	cl_assert(exp.files == 1);
	cl_assert(exp.hunks == 1);
	cl_assert(exp.lines == 15);
	cl_assert(exp.line_ctxt == 3);
	cl_assert(exp.line_adds == 9);
	cl_assert(exp.line_dels == 3);

	memset(&exp, 0, sizeof(exp));
	cl_git_pass(git_diff_blobs(g_repo, a, c, &opts));

	cl_assert(exp.files == 1);
	cl_assert(exp.hunks == 1);
	cl_assert(exp.lines == 13);
	cl_assert(exp.line_ctxt == 0);
	cl_assert(exp.line_adds == 12);
	cl_assert(exp.line_dels == 1);

	opts.context_lines = 2;

	memset(&exp, 0, sizeof(exp));
	cl_git_pass(git_diff_blobs(g_repo, c, d, &opts));

	cl_assert(exp.files == 1);
	cl_assert(exp.hunks == 2);
	cl_assert(exp.lines == 16);
	cl_assert(exp.line_ctxt == 6);
	cl_assert(exp.line_adds == 6);
	cl_assert(exp.line_dels == 4);

	git_blob_free(a);
	git_blob_free(b);
	git_blob_free(c);
}

