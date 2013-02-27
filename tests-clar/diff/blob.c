#include "clar_libgit2.h"
#include "diff_helpers.h"

static git_repository *g_repo = NULL;
static diff_expects expected;
static git_diff_options opts;
static git_blob *d, *alien;

void test_diff_blob__initialize(void)
{
	git_oid oid;

	g_repo = cl_git_sandbox_init("attr");

	GIT_INIT_STRUCTURE(&opts, GIT_DIFF_OPTIONS_VERSION);
	opts.context_lines = 1;
	opts.interhunk_lines = 0;

	memset(&expected, 0, sizeof(expected));

	/* tests/resources/attr/root_test4.txt */
	cl_git_pass(git_oid_fromstrn(&oid, "a0f7217a", 8));
	cl_git_pass(git_blob_lookup_prefix(&d, g_repo, &oid, 4));

	/* alien.png */
	cl_git_pass(git_oid_fromstrn(&oid, "edf3dcee", 8));
	cl_git_pass(git_blob_lookup_prefix(&alien, g_repo, &oid, 4));
}

void test_diff_blob__cleanup(void)
{
	git_blob_free(d);
	d = NULL;

	git_blob_free(alien);
	alien = NULL;

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

	/* diff on tests/resources/attr/root_test1 */
	cl_git_pass(git_diff_blobs(
		a, b, &opts, diff_file_cb, diff_hunk_cb, diff_line_cb, &expected));

	cl_assert_equal_i(1, expected.files);
	cl_assert_equal_i(1, expected.file_status[GIT_DELTA_MODIFIED]);
	cl_assert_equal_i(0, expected.files_binary);

	cl_assert_equal_i(1, expected.hunks);
	cl_assert_equal_i(6, expected.lines);
	cl_assert_equal_i(1, expected.line_ctxt);
	cl_assert_equal_i(5, expected.line_adds);
	cl_assert_equal_i(0, expected.line_dels);

	/* diff on tests/resources/attr/root_test2 */
	memset(&expected, 0, sizeof(expected));
	cl_git_pass(git_diff_blobs(
		b, c, &opts, diff_file_cb, diff_hunk_cb, diff_line_cb, &expected));

	cl_assert_equal_i(1, expected.files);
	cl_assert_equal_i(1, expected.file_status[GIT_DELTA_MODIFIED]);
	cl_assert_equal_i(0, expected.files_binary);

	cl_assert_equal_i(1, expected.hunks);
	cl_assert_equal_i(15, expected.lines);
	cl_assert_equal_i(3, expected.line_ctxt);
	cl_assert_equal_i(9, expected.line_adds);
	cl_assert_equal_i(3, expected.line_dels);

	/* diff on tests/resources/attr/root_test3 */
	memset(&expected, 0, sizeof(expected));
	cl_git_pass(git_diff_blobs(
		a, c, &opts, diff_file_cb, diff_hunk_cb, diff_line_cb, &expected));

	cl_assert_equal_i(1, expected.files);
	cl_assert_equal_i(1, expected.file_status[GIT_DELTA_MODIFIED]);
	cl_assert_equal_i(0, expected.files_binary);

	cl_assert_equal_i(1, expected.hunks);
	cl_assert_equal_i(13, expected.lines);
	cl_assert_equal_i(0, expected.line_ctxt);
	cl_assert_equal_i(12, expected.line_adds);
	cl_assert_equal_i(1, expected.line_dels);

	memset(&expected, 0, sizeof(expected));
	cl_git_pass(git_diff_blobs(
		c, d, &opts, diff_file_cb, diff_hunk_cb, diff_line_cb, &expected));

	cl_assert_equal_i(1, expected.files);
	cl_assert_equal_i(1, expected.file_status[GIT_DELTA_MODIFIED]);
	cl_assert_equal_i(0, expected.files_binary);

	cl_assert_equal_i(2, expected.hunks);
	cl_assert_equal_i(14, expected.lines);
	cl_assert_equal_i(4, expected.line_ctxt);
	cl_assert_equal_i(6, expected.line_adds);
	cl_assert_equal_i(4, expected.line_dels);

	git_blob_free(a);
	git_blob_free(b);
	git_blob_free(c);
}

void test_diff_blob__can_compare_against_null_blobs(void)
{
	git_blob *e = NULL;

	cl_git_pass(git_diff_blobs(
		d, e, &opts, diff_file_cb, diff_hunk_cb, diff_line_cb, &expected));

	cl_assert_equal_i(1, expected.files);
	cl_assert_equal_i(1, expected.file_status[GIT_DELTA_DELETED]);
	cl_assert_equal_i(0, expected.files_binary);

	cl_assert_equal_i(1, expected.hunks);
	cl_assert_equal_i(14, expected.hunk_old_lines);
	cl_assert_equal_i(14, expected.lines);
	cl_assert_equal_i(14, expected.line_dels);

	opts.flags |= GIT_DIFF_REVERSE;
	memset(&expected, 0, sizeof(expected));

	cl_git_pass(git_diff_blobs(
		d, e, &opts, diff_file_cb, diff_hunk_cb, diff_line_cb, &expected));

	cl_assert_equal_i(1, expected.files);
	cl_assert_equal_i(1, expected.file_status[GIT_DELTA_ADDED]);
	cl_assert_equal_i(0, expected.files_binary);

	cl_assert_equal_i(1, expected.hunks);
	cl_assert_equal_i(14, expected.hunk_new_lines);
	cl_assert_equal_i(14, expected.lines);
	cl_assert_equal_i(14, expected.line_adds);

	opts.flags ^= GIT_DIFF_REVERSE;
	memset(&expected, 0, sizeof(expected));

	cl_git_pass(git_diff_blobs(
		alien, NULL, &opts, diff_file_cb, diff_hunk_cb, diff_line_cb, &expected));

	cl_assert_equal_i(1, expected.files);
	cl_assert_equal_i(1, expected.files_binary);
	cl_assert_equal_i(1, expected.file_status[GIT_DELTA_DELETED]);
	cl_assert_equal_i(0, expected.hunks);
	cl_assert_equal_i(0, expected.lines);

	memset(&expected, 0, sizeof(expected));

	cl_git_pass(git_diff_blobs(
		NULL, alien, &opts, diff_file_cb, diff_hunk_cb, diff_line_cb, &expected));

	cl_assert_equal_i(1, expected.files);
	cl_assert_equal_i(1, expected.files_binary);
	cl_assert_equal_i(1, expected.file_status[GIT_DELTA_ADDED]);
	cl_assert_equal_i(0, expected.hunks);
	cl_assert_equal_i(0, expected.lines);
}

static void assert_identical_blobs_comparison(diff_expects *expected)
{
	cl_assert_equal_i(1, expected->files);
	cl_assert_equal_i(1, expected->file_status[GIT_DELTA_UNMODIFIED]);
	cl_assert_equal_i(0, expected->hunks);
	cl_assert_equal_i(0, expected->lines);
}

void test_diff_blob__can_compare_identical_blobs(void)
{
	cl_git_pass(git_diff_blobs(
		d, d, &opts, diff_file_cb, diff_hunk_cb, diff_line_cb, &expected));

	cl_assert_equal_i(0, expected.files_binary);
	assert_identical_blobs_comparison(&expected);

	memset(&expected, 0, sizeof(expected));
	cl_git_pass(git_diff_blobs(
		NULL, NULL, &opts, diff_file_cb, diff_hunk_cb, diff_line_cb, &expected));

	cl_assert_equal_i(0, expected.files_binary);
	cl_assert_equal_i(0, expected.files); /* NULLs mean no callbacks, period */

	memset(&expected, 0, sizeof(expected));
	cl_git_pass(git_diff_blobs(
		alien, alien, &opts, diff_file_cb, diff_hunk_cb, diff_line_cb, &expected));

	cl_assert(expected.files_binary > 0);
	assert_identical_blobs_comparison(&expected);
}

static void assert_binary_blobs_comparison(diff_expects *expected)
{
	cl_assert(expected->files_binary > 0);

	cl_assert_equal_i(1, expected->files);
	cl_assert_equal_i(1, expected->file_status[GIT_DELTA_MODIFIED]);
	cl_assert_equal_i(0, expected->hunks);
	cl_assert_equal_i(0, expected->lines);
}

void test_diff_blob__can_compare_two_binary_blobs(void)
{
	git_blob *heart;
	git_oid h_oid;

	/* heart.png */
	cl_git_pass(git_oid_fromstrn(&h_oid, "de863bff", 8));
	cl_git_pass(git_blob_lookup_prefix(&heart, g_repo, &h_oid, 4));

	cl_git_pass(git_diff_blobs(
		alien, heart, &opts, diff_file_cb, diff_hunk_cb, diff_line_cb, &expected));

	assert_binary_blobs_comparison(&expected);

	memset(&expected, 0, sizeof(expected));

	cl_git_pass(git_diff_blobs(
		heart, alien, &opts, diff_file_cb, diff_hunk_cb, diff_line_cb, &expected));

	assert_binary_blobs_comparison(&expected);

	git_blob_free(heart);
}

void test_diff_blob__can_compare_a_binary_blob_and_a_text_blob(void)
{
	cl_git_pass(git_diff_blobs(
		alien, d, &opts, diff_file_cb, diff_hunk_cb, diff_line_cb, &expected));

	assert_binary_blobs_comparison(&expected);

	memset(&expected, 0, sizeof(expected));

	cl_git_pass(git_diff_blobs(
		d, alien, &opts, diff_file_cb, diff_hunk_cb, diff_line_cb, &expected));

	assert_binary_blobs_comparison(&expected);
}

/*
 * $ git diff fe773770 a0f7217
 * diff --git a/fe773770 b/a0f7217
 * index fe77377..a0f7217 100644
 * --- a/fe773770
 * +++ b/a0f7217
 * @@ -1,6 +1,6 @@
 *  Here is some stuff at the start
 * 
 * -This should go in one hunk
 * +This should go in one hunk (first)
 * 
 *  Some additional lines
 * 
 * @@ -8,7 +8,7 @@ Down here below the other lines
 * 
 *  With even more at the end
 * 
 * -Followed by a second hunk of stuff
 * +Followed by a second hunk of stuff (second)
 * 
 *  That happens down here
 */
void test_diff_blob__comparing_two_text_blobs_honors_interhunkcontext(void)
{
	git_blob *old_d;
	git_oid old_d_oid;

	opts.context_lines = 3;

	/* tests/resources/attr/root_test1 from commit f5b0af1 */
	cl_git_pass(git_oid_fromstrn(&old_d_oid, "fe773770", 8));
	cl_git_pass(git_blob_lookup_prefix(&old_d, g_repo, &old_d_oid, 4));

	/* Test with default inter-hunk-context (not set) => default is 0 */
	cl_git_pass(git_diff_blobs(
		old_d, d, &opts, diff_file_cb, diff_hunk_cb, diff_line_cb, &expected));

	cl_assert_equal_i(2, expected.hunks);

	/* Test with inter-hunk-context explicitly set to 0 */
	opts.interhunk_lines = 0;
	memset(&expected, 0, sizeof(expected));
	cl_git_pass(git_diff_blobs(
		old_d, d, &opts, diff_file_cb, diff_hunk_cb, diff_line_cb, &expected));

	cl_assert_equal_i(2, expected.hunks);

	/* Test with inter-hunk-context explicitly set to 1 */
	opts.interhunk_lines = 1;
	memset(&expected, 0, sizeof(expected));
	cl_git_pass(git_diff_blobs(
		old_d, d, &opts, diff_file_cb, diff_hunk_cb, diff_line_cb, &expected));

	cl_assert_equal_i(1, expected.hunks);

	git_blob_free(old_d);
}

void test_diff_blob__checks_options_version_too_low(void)
{
	const git_error *err;

	opts.version = 0;
	cl_git_fail(git_diff_blobs(
		d, alien, &opts, diff_file_cb, diff_hunk_cb, diff_line_cb, &expected));
	err = giterr_last();
	cl_assert_equal_i(GITERR_INVALID, err->klass);
}

void test_diff_blob__checks_options_version_too_high(void)
{
	const git_error *err;

	opts.version = 1024;
	cl_git_fail(git_diff_blobs(
		d, alien, &opts, diff_file_cb, diff_hunk_cb, diff_line_cb, &expected));
	err = giterr_last();
	cl_assert_equal_i(GITERR_INVALID, err->klass);
}

void test_diff_blob__can_correctly_detect_a_binary_blob_as_binary(void)
{
	/* alien.png */
	cl_assert_equal_i(true, git_blob_is_binary(alien));
}

void test_diff_blob__can_correctly_detect_a_textual_blob_as_non_binary(void)
{
	/* tests/resources/attr/root_test4.txt */
	cl_assert_equal_i(false, git_blob_is_binary(d));
}

/*
 * git_diff_blob_to_buffer tests
 */

static void assert_changed_single_one_line_file(
	diff_expects *expected, git_delta_t mod)
{
	cl_assert_equal_i(1, expected->files);
	cl_assert_equal_i(1, expected->file_status[mod]);
	cl_assert_equal_i(1, expected->hunks);
	cl_assert_equal_i(1, expected->lines);

	if (mod == GIT_DELTA_ADDED)
		cl_assert_equal_i(1, expected->line_adds);
	else if (mod == GIT_DELTA_DELETED)
		cl_assert_equal_i(1, expected->line_dels);
}

void test_diff_blob__can_compare_blob_to_buffer(void)
{
	git_blob *a;
	git_oid a_oid;
	const char *a_content = "Hello from the root\n";
	const char *b_content = "Hello from the root\n\nSome additional lines\n\nDown here below\n\n";

	/* tests/resources/attr/root_test1 */
	cl_git_pass(git_oid_fromstrn(&a_oid, "45141a79", 8));
	cl_git_pass(git_blob_lookup_prefix(&a, g_repo, &a_oid, 4));

	/* diff from blob a to content of b */
	cl_git_pass(git_diff_blob_to_buffer(
		a, b_content, strlen(b_content),
		&opts, diff_file_cb, diff_hunk_cb, diff_line_cb, &expected));

	cl_assert_equal_i(1, expected.files);
	cl_assert_equal_i(1, expected.file_status[GIT_DELTA_MODIFIED]);
	cl_assert_equal_i(0, expected.files_binary);
	cl_assert_equal_i(1, expected.hunks);
	cl_assert_equal_i(6, expected.lines);
	cl_assert_equal_i(1, expected.line_ctxt);
	cl_assert_equal_i(5, expected.line_adds);
	cl_assert_equal_i(0, expected.line_dels);

	/* diff from blob a to content of a */
	memset(&expected, 0, sizeof(expected));
	cl_git_pass(git_diff_blob_to_buffer(
		a, a_content, strlen(a_content),
		&opts, diff_file_cb, diff_hunk_cb, diff_line_cb, &expected));

	assert_identical_blobs_comparison(&expected);

	/* diff from NULL blob to content of a */
	memset(&expected, 0, sizeof(expected));
	cl_git_pass(git_diff_blob_to_buffer(
		NULL, a_content, strlen(a_content),
		&opts, diff_file_cb, diff_hunk_cb, diff_line_cb, &expected));

	assert_changed_single_one_line_file(&expected, GIT_DELTA_ADDED);

	/* diff from blob a to NULL buffer */
	memset(&expected, 0, sizeof(expected));
	cl_git_pass(git_diff_blob_to_buffer(
		a, NULL, 0,
		&opts, diff_file_cb, diff_hunk_cb, diff_line_cb, &expected));

	assert_changed_single_one_line_file(&expected, GIT_DELTA_DELETED);

	/* diff with reverse */
	opts.flags ^= GIT_DIFF_REVERSE;

	memset(&expected, 0, sizeof(expected));
	cl_git_pass(git_diff_blob_to_buffer(
		a, NULL, 0,
		&opts, diff_file_cb, diff_hunk_cb, diff_line_cb, &expected));

	assert_changed_single_one_line_file(&expected, GIT_DELTA_ADDED);

	git_blob_free(a);
}


static void assert_one_modified_with_lines(diff_expects *expected, int lines)
{
	cl_assert_equal_i(1, expected->files);
	cl_assert_equal_i(1, expected->file_status[GIT_DELTA_MODIFIED]);
	cl_assert_equal_i(0, expected->files_binary);
	cl_assert_equal_i(lines, expected->lines);
}

void test_diff_blob__binary_data_comparisons(void)
{
	git_blob *bin, *nonbin;
	git_oid oid;
	const char *nonbin_content = "Hello from the root\n";
	size_t nonbin_len = 20;
	const char *bin_content = "0123456789\n\x01\x02\x03\x04\x05\x06\x07\x08\x09\x00\n0123456789\n";
	size_t bin_len = 33;

	cl_git_pass(git_oid_fromstrn(&oid, "45141a79", 8));
	cl_git_pass(git_blob_lookup_prefix(&nonbin, g_repo, &oid, 4));

	cl_git_pass(git_oid_fromstrn(&oid, "b435cd56", 8));
	cl_git_pass(git_blob_lookup_prefix(&bin, g_repo, &oid, 4));

	/* non-binary to reference content */

	memset(&expected, 0, sizeof(expected));
	cl_git_pass(git_diff_blob_to_buffer(
		nonbin, nonbin_content, nonbin_len,
		&opts, diff_file_cb, diff_hunk_cb, diff_line_cb, &expected));
	assert_identical_blobs_comparison(&expected);
	cl_assert_equal_i(0, expected.files_binary);

	/* binary to reference content */

	memset(&expected, 0, sizeof(expected));
	cl_git_pass(git_diff_blob_to_buffer(
		bin, bin_content, bin_len,
		&opts, diff_file_cb, diff_hunk_cb, diff_line_cb, &expected));
	assert_identical_blobs_comparison(&expected);

	cl_assert_equal_i(1, expected.files_binary);

	/* non-binary to binary content */

	memset(&expected, 0, sizeof(expected));
	cl_git_pass(git_diff_blob_to_buffer(
		nonbin, bin_content, bin_len,
		&opts, diff_file_cb, diff_hunk_cb, diff_line_cb, &expected));
	assert_binary_blobs_comparison(&expected);

	/* binary to non-binary content */

	memset(&expected, 0, sizeof(expected));
	cl_git_pass(git_diff_blob_to_buffer(
		bin, nonbin_content, nonbin_len,
		&opts, diff_file_cb, diff_hunk_cb, diff_line_cb, &expected));
	assert_binary_blobs_comparison(&expected);

	/* non-binary to binary blob */

	memset(&expected, 0, sizeof(expected));
	cl_git_pass(git_diff_blobs(
		bin, nonbin, &opts,
		diff_file_cb, diff_hunk_cb, diff_line_cb, &expected));
	assert_binary_blobs_comparison(&expected);

	/*
	 * repeat with FORCE_TEXT
	 */

	opts.flags |= GIT_DIFF_FORCE_TEXT;

	memset(&expected, 0, sizeof(expected));
	cl_git_pass(git_diff_blob_to_buffer(
		bin, bin_content, bin_len,
		&opts, diff_file_cb, diff_hunk_cb, diff_line_cb, &expected));
	assert_identical_blobs_comparison(&expected);

	memset(&expected, 0, sizeof(expected));
	cl_git_pass(git_diff_blob_to_buffer(
		nonbin, bin_content, bin_len,
		&opts, diff_file_cb, diff_hunk_cb, diff_line_cb, &expected));
	assert_one_modified_with_lines(&expected, 4);

	memset(&expected, 0, sizeof(expected));
	cl_git_pass(git_diff_blob_to_buffer(
		bin, nonbin_content, nonbin_len,
		&opts, diff_file_cb, diff_hunk_cb, diff_line_cb, &expected));
	assert_one_modified_with_lines(&expected, 4);

	memset(&expected, 0, sizeof(expected));
	cl_git_pass(git_diff_blobs(
		bin, nonbin, &opts,
		diff_file_cb, diff_hunk_cb, diff_line_cb, &expected));
	assert_one_modified_with_lines(&expected, 4);

	/* cleanup */
	git_blob_free(bin);
	git_blob_free(nonbin);
}
