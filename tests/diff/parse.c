#include "clar_libgit2.h"
#include "patch.h"
#include "patch_parse.h"

#include "../patch/patch_common.h"

void test_diff_parse__nonpatches_fail_with_notfound(void)
{
	git_diff *diff;
	const char *not = PATCH_NOT_A_PATCH;
	const char *not_with_leading = "Leading text.\n" PATCH_NOT_A_PATCH;
	const char *not_with_trailing = PATCH_NOT_A_PATCH "Trailing text.\n";
	const char *not_with_both = "Lead.\n" PATCH_NOT_A_PATCH "Trail.\n";

	cl_git_fail_with(GIT_ENOTFOUND,
		git_diff_from_buffer(&diff,
		not,
		strlen(not)));
	cl_git_fail_with(GIT_ENOTFOUND,
		git_diff_from_buffer(&diff,
		not_with_leading,
		strlen(not_with_leading)));
	cl_git_fail_with(GIT_ENOTFOUND,
		git_diff_from_buffer(&diff,
		not_with_trailing,
		strlen(not_with_trailing)));
	cl_git_fail_with(GIT_ENOTFOUND,
		git_diff_from_buffer(&diff,
		not_with_both,
		strlen(not_with_both)));
}

static void test_parse_invalid_diff(const char *invalid_diff)
{
	git_diff *diff;
	git_buf buf = GIT_BUF_INIT;

	/* throw some random (legitimate) diffs in with the given invalid
	 * one.
	 */
	git_buf_puts(&buf, PATCH_ORIGINAL_TO_CHANGE_FIRSTLINE);
	git_buf_puts(&buf, PATCH_BINARY_DELTA);
	git_buf_puts(&buf, invalid_diff);
	git_buf_puts(&buf, PATCH_ORIGINAL_TO_CHANGE_MIDDLE);
	git_buf_puts(&buf, PATCH_BINARY_LITERAL);

	cl_git_fail_with(GIT_ERROR,
		git_diff_from_buffer(&diff, buf.ptr, buf.size));

	git_buf_free(&buf);
}

void test_diff_parse__invalid_patches_fails(void)
{
	test_parse_invalid_diff(PATCH_CORRUPT_MISSING_NEW_FILE);
	test_parse_invalid_diff(PATCH_CORRUPT_MISSING_OLD_FILE);
	test_parse_invalid_diff(PATCH_CORRUPT_NO_CHANGES);
	test_parse_invalid_diff(PATCH_CORRUPT_MISSING_HUNK_HEADER);
}

