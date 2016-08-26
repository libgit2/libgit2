#include "clar_libgit2.h"
#include "diff_helpers.h"

static git_repository *g_repo = NULL;

void test_diff_blob__initialize(void)
{
	g_repo = cl_git_sandbox_init("attr");
}

void test_diff_blob__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

void test_diff_blob__0(void)
{
	git_blob *a, *b, *c, *d;
	git_oid a_oid, b_oid, c_oid, d_oid;
	git_diff_options opts = {0};
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

	/* Doing the equivalent of a `git diff -U1` on these files */

	opts.context_lines = 1;
	opts.interhunk_lines = 1;

	memset(&exp, 0, sizeof(exp));
	cl_git_pass(git_diff_blobs(
		a, b, &opts, &exp, diff_hunk_fn, diff_line_fn));

	cl_assert(exp.hunks == 1);
	cl_assert(exp.lines == 6);
	cl_assert(exp.line_ctxt == 1);
	cl_assert(exp.line_adds == 5);
	cl_assert(exp.line_dels == 0);

	memset(&exp, 0, sizeof(exp));
	cl_git_pass(git_diff_blobs(
		b, c, &opts, &exp, diff_hunk_fn, diff_line_fn));

	cl_assert(exp.hunks == 1);
	cl_assert(exp.lines == 15);
	cl_assert(exp.line_ctxt == 3);
	cl_assert(exp.line_adds == 9);
	cl_assert(exp.line_dels == 3);

	memset(&exp, 0, sizeof(exp));
	cl_git_pass(git_diff_blobs(
		a, c, &opts, &exp, diff_hunk_fn, diff_line_fn));

	cl_assert(exp.hunks == 1);
	cl_assert(exp.lines == 13);
	cl_assert(exp.line_ctxt == 0);
	cl_assert(exp.line_adds == 12);
	cl_assert(exp.line_dels == 1);

	opts.context_lines = 1;

	memset(&exp, 0, sizeof(exp));
	cl_git_pass(git_diff_blobs(
		c, d, &opts, &exp, diff_hunk_fn, diff_line_fn));

	cl_assert(exp.hunks == 2);
	cl_assert(exp.lines == 14);
	cl_assert(exp.line_ctxt == 4);
	cl_assert(exp.line_adds == 6);
	cl_assert(exp.line_dels == 4);

	git_blob_free(a);
	git_blob_free(b);
	git_blob_free(c);
	git_blob_free(d);
}

