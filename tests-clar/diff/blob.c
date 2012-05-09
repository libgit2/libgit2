#include "clar_libgit2.h"
#include "diff_helpers.h"

static git_repository *g_repo = NULL;
static diff_expects exp;
static git_diff_options opts;
static git_blob *d, *alien;

void test_diff_blob__initialize(void)
{
	git_oid oid;

	g_repo = cl_git_sandbox_init("attr");

	memset(&opts, 0, sizeof(opts));
	opts.context_lines = 1;
	opts.interhunk_lines = 1;

	memset(&exp, 0, sizeof(exp));

	/* tests/resources/attr/root_test4.txt */
	cl_git_pass(git_oid_fromstrn(&oid, "fe773770c5a6", 12));
	cl_git_pass(git_blob_lookup_prefix(&d, g_repo, &oid, 6));

	/* alien.png */
	cl_git_pass(git_oid_fromstrn(&oid, "edf3dcee", 8));
	cl_git_pass(git_blob_lookup_prefix(&alien, g_repo, &oid, 4));
}

void test_diff_blob__cleanup(void)
{
	git_blob_free(d);
	git_blob_free(alien);

	cl_git_sandbox_cleanup();
}

void test_diff_blob__can_compare_text_blobs(void)
{
	git_blob *a, *b, *c;
	git_oid a_oid, b_oid, c_oid;

	/* tests/resources/attr/root_test1 */
	cl_git_pass(git_oid_fromstrn(&a_oid, "45141a79", 8));
	cl_git_pass(git_blob_lookup_prefix(&a, g_repo, &a_oid, 4));

	/* tests/resources/attr/root_test2 */
	cl_git_pass(git_oid_fromstrn(&b_oid, "4d713dc4", 8));
	cl_git_pass(git_blob_lookup_prefix(&b, g_repo, &b_oid, 4));

	/* tests/resources/attr/root_test3 */
	cl_git_pass(git_oid_fromstrn(&c_oid, "c96bbb2c2557a832", 16));
	cl_git_pass(git_blob_lookup_prefix(&c, g_repo, &c_oid, 8));

	/* Doing the equivalent of a `git diff -U1` on these files */

	cl_git_pass(git_diff_blobs(
		a, b, &opts, &exp, diff_file_fn, diff_hunk_fn, diff_line_fn));

	cl_assert(exp.files == 1);
	cl_assert(exp.file_mods == 1);
	cl_assert(exp.at_least_one_of_them_is_binary == false);

	cl_assert(exp.hunks == 1);
	cl_assert(exp.lines == 6);
	cl_assert(exp.line_ctxt == 1);
	cl_assert(exp.line_adds == 5);
	cl_assert(exp.line_dels == 0);

	memset(&exp, 0, sizeof(exp));
	cl_git_pass(git_diff_blobs(
		b, c, &opts, &exp, diff_file_fn, diff_hunk_fn, diff_line_fn));

	cl_assert(exp.files == 1);
	cl_assert(exp.file_mods == 1);
	cl_assert(exp.at_least_one_of_them_is_binary == false);

	cl_assert(exp.hunks == 1);
	cl_assert(exp.lines == 15);
	cl_assert(exp.line_ctxt == 3);
	cl_assert(exp.line_adds == 9);
	cl_assert(exp.line_dels == 3);

	memset(&exp, 0, sizeof(exp));
	cl_git_pass(git_diff_blobs(
		a, c, &opts, &exp, diff_file_fn, diff_hunk_fn, diff_line_fn));

	cl_assert(exp.files == 1);
	cl_assert(exp.file_mods == 1);
	cl_assert(exp.at_least_one_of_them_is_binary == false);

	cl_assert(exp.hunks == 1);
	cl_assert(exp.lines == 13);
	cl_assert(exp.line_ctxt == 0);
	cl_assert(exp.line_adds == 12);
	cl_assert(exp.line_dels == 1);

	opts.context_lines = 1;

	memset(&exp, 0, sizeof(exp));
	cl_git_pass(git_diff_blobs(
		c, d, &opts, &exp, diff_file_fn, diff_hunk_fn, diff_line_fn));

	cl_assert(exp.files == 1);
	cl_assert(exp.file_mods == 1);
	cl_assert(exp.at_least_one_of_them_is_binary == false);

	cl_assert(exp.hunks == 2);
	cl_assert(exp.lines == 14);
	cl_assert(exp.line_ctxt == 4);
	cl_assert(exp.line_adds == 6);
	cl_assert(exp.line_dels == 4);

	git_blob_free(a);
	git_blob_free(b);
	git_blob_free(c);
}

void test_diff_blob__can_compare_against_null_blobs(void)
{
	git_blob *e = NULL;

	cl_git_pass(git_diff_blobs(
		d, e, &opts, &exp, diff_file_fn, diff_hunk_fn, diff_line_fn));

	cl_assert(exp.files == 1);
	cl_assert(exp.file_dels == 1);
	cl_assert(exp.at_least_one_of_them_is_binary == false);

	cl_assert(exp.hunks == 1);
	cl_assert(exp.hunk_old_lines == 14);
	cl_assert(exp.lines == 14);
	cl_assert(exp.line_dels == 14);

	opts.flags |= GIT_DIFF_REVERSE;
	memset(&exp, 0, sizeof(exp));

	cl_git_pass(git_diff_blobs(
		d, e, &opts, &exp, diff_file_fn, diff_hunk_fn, diff_line_fn));

	cl_assert(exp.files == 1);
	cl_assert(exp.file_adds == 1);
	cl_assert(exp.at_least_one_of_them_is_binary == false);

	cl_assert(exp.hunks == 1);
	cl_assert(exp.hunk_new_lines == 14);
	cl_assert(exp.lines == 14);
	cl_assert(exp.line_adds == 14);

	opts.flags ^= GIT_DIFF_REVERSE;
	memset(&exp, 0, sizeof(exp));

	cl_git_pass(git_diff_blobs(
		alien, NULL, &opts, &exp, diff_file_fn, diff_hunk_fn, diff_line_fn));

	cl_assert(exp.at_least_one_of_them_is_binary == true);

	cl_assert(exp.files == 1);
	cl_assert(exp.file_dels == 1);
	cl_assert(exp.hunks == 0);
	cl_assert(exp.lines == 0);

	memset(&exp, 0, sizeof(exp));

	cl_git_pass(git_diff_blobs(
		NULL, alien, &opts, &exp, diff_file_fn, diff_hunk_fn, diff_line_fn));

	cl_assert(exp.at_least_one_of_them_is_binary == true);

	cl_assert(exp.files == 1);
	cl_assert(exp.file_adds == 1);
	cl_assert(exp.hunks == 0);
	cl_assert(exp.lines == 0);
}

static void assert_identical_blobs_comparison(diff_expects exp)
{
	cl_assert(exp.files == 1);
	cl_assert(exp.file_unmodified == 1);
	cl_assert(exp.hunks == 0);
	cl_assert(exp.lines == 0);
}

void test_diff_blob__can_compare_identical_blobs(void)
{
	cl_git_pass(git_diff_blobs(
		d, d, &opts, &exp, diff_file_fn, diff_hunk_fn, diff_line_fn));

	cl_assert(exp.at_least_one_of_them_is_binary == false);
	assert_identical_blobs_comparison(exp);

	memset(&exp, 0, sizeof(exp));
	cl_git_pass(git_diff_blobs(
		NULL, NULL, &opts, &exp, diff_file_fn, diff_hunk_fn, diff_line_fn));

	cl_assert(exp.at_least_one_of_them_is_binary == false);
	assert_identical_blobs_comparison(exp);

	memset(&exp, 0, sizeof(exp));
	cl_git_pass(git_diff_blobs(
		alien, alien, &opts, &exp, diff_file_fn, diff_hunk_fn, diff_line_fn));

	cl_assert(exp.at_least_one_of_them_is_binary == true);
	assert_identical_blobs_comparison(exp);
}

static void assert_binary_blobs_comparison(diff_expects exp)
{
	cl_assert(exp.at_least_one_of_them_is_binary == true);

	cl_assert(exp.files == 1);
	cl_assert(exp.file_mods == 1);
	cl_assert(exp.hunks == 0);
	cl_assert(exp.lines == 0);
}

void test_diff_blob__can_compare_two_binary_blobs(void)
{
	git_blob *heart;
	git_oid h_oid;

	/* heart.png */
	cl_git_pass(git_oid_fromstrn(&h_oid, "de863bff", 8));
	cl_git_pass(git_blob_lookup_prefix(&heart, g_repo, &h_oid, 4));

	cl_git_pass(git_diff_blobs(
		alien, heart, &opts, &exp, diff_file_fn, diff_hunk_fn, diff_line_fn));

	assert_binary_blobs_comparison(exp);

	memset(&exp, 0, sizeof(exp));

	cl_git_pass(git_diff_blobs(
		heart, alien, &opts, &exp, diff_file_fn, diff_hunk_fn, diff_line_fn));

	assert_binary_blobs_comparison(exp);

	git_blob_free(heart);
}

void test_diff_blob__can_compare_a_binary_blob_and_a_text_blob(void)
{
	cl_git_pass(git_diff_blobs(
		alien, d, &opts, &exp, diff_file_fn, diff_hunk_fn, diff_line_fn));

	assert_binary_blobs_comparison(exp);

	memset(&exp, 0, sizeof(exp));

	cl_git_pass(git_diff_blobs(
		d, alien, &opts, &exp, diff_file_fn, diff_hunk_fn, diff_line_fn));

	assert_binary_blobs_comparison(exp);
}
