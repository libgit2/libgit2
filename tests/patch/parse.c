#include "clar_libgit2.h"
#include "patch.h"
#include "patch_parse.h"

#include "patch_common.h"


static void ensure_patch_validity_null_side(
		char *idstr, const git_diff_file *side, const git_diff_file *other) {
	cl_assert(side->path != NULL);
	cl_assert_equal_s(side->path, other->path);
	cl_assert(side->mode == GIT_FILEMODE_UNREADABLE);
	cl_assert_equal_i(0, side->id_abbrev);
	git_oid_nfmt(idstr, side->id_abbrev, &side->id);
	cl_assert_equal_s(idstr, "");
	cl_assert_equal_i(0, side->size);
}

static void ensure_patch_validity_general(
	git_patch *patch,
	char *name,
	char *old_idstr,
	char *new_idstr)
{
	const git_diff_delta *delta;
	char idstr[GIT_OID_HEXSZ+1] = {0};

	cl_assert((delta = git_patch_get_delta(patch)) != NULL);
	cl_assert_equal_i(old_idstr[0] && new_idstr[0] ? 2 : 1, delta->nfiles);

	if (!old_idstr[0]) {
		cl_assert(delta->status == GIT_DELTA_ADDED);
		ensure_patch_validity_null_side(idstr, &delta->old_file, &delta->new_file);
	} else {
		cl_assert_equal_s(delta->old_file.path, name);
		cl_assert(delta->old_file.mode == GIT_FILEMODE_BLOB);
		cl_assert_equal_i(strlen(old_idstr), delta->old_file.id_abbrev);
		git_oid_nfmt(idstr, delta->old_file.id_abbrev, &delta->old_file.id);
		cl_assert_equal_s(idstr, old_idstr);
		cl_assert_equal_i(0, delta->old_file.size);
	}

	idstr[0] = '\0';

	if (!new_idstr[0]) {
		cl_assert(delta->status == GIT_DELTA_DELETED);
		ensure_patch_validity_null_side(idstr, &delta->new_file, &delta->old_file);
	} else {
		cl_assert_equal_s(delta->new_file.path, name);
		cl_assert(delta->new_file.mode == GIT_FILEMODE_BLOB);
		cl_assert_equal_i(strlen(new_idstr), delta->new_file.id_abbrev);
		git_oid_nfmt(idstr, delta->new_file.id_abbrev, &delta->new_file.id);
		cl_assert_equal_s(idstr, new_idstr);
		cl_assert_equal_i(0, delta->new_file.size);
	}
}

#define ensure_patch_validity(patch) \
	ensure_patch_validity_general(patch, "file.txt", "9432026", "cd8fd12");

static void ensure_identical_patch_inout(const char *content) {
	git_buf buf = GIT_BUF_INIT;
	git_patch *patch;

	cl_git_pass(git_patch_from_buffer(&patch, content, strlen(content), NULL));
	cl_git_pass(git_patch_to_buf(&buf, patch));
	cl_assert_equal_strn(git_buf_cstr(&buf), content, strlen(content));

	git_patch_free(patch);
	git_buf_dispose(&buf);
}

void test_patch_parse__original_to_change_middle(void)
{
	git_patch *patch;

	cl_git_pass(git_patch_from_buffer(
		&patch, PATCH_ORIGINAL_TO_CHANGE_MIDDLE,
		strlen(PATCH_ORIGINAL_TO_CHANGE_MIDDLE), NULL));
	ensure_patch_validity(patch);
	git_patch_free(patch);
}

void test_patch_parse__leading_and_trailing_garbage(void)
{
	git_patch *patch;
	const char *leading = "This is some leading garbage.\n"
		"Maybe it's email headers?\n"
		"\n"
		PATCH_ORIGINAL_TO_CHANGE_MIDDLE;
	const char *trailing = PATCH_ORIGINAL_TO_CHANGE_MIDDLE
		"\n"
		"This is some trailing garbage.\n"
		"Maybe it's an email signature?\n";
	const char *both = "Here's some leading garbage\n"
		PATCH_ORIGINAL_TO_CHANGE_MIDDLE
		"And here's some trailing.\n";

	cl_git_pass(git_patch_from_buffer(&patch, leading, strlen(leading),
		NULL));
	ensure_patch_validity(patch);
	git_patch_free(patch);

	cl_git_pass(git_patch_from_buffer(&patch, trailing, strlen(trailing),
		NULL));
	ensure_patch_validity(patch);
	git_patch_free(patch);

	cl_git_pass(git_patch_from_buffer(&patch, both, strlen(both),
		NULL));
	ensure_patch_validity(patch);
	git_patch_free(patch);
}

void test_patch_parse__nonpatches_fail_with_notfound(void)
{
	git_patch *patch;

	cl_git_fail_with(GIT_ENOTFOUND,
		git_patch_from_buffer(&patch, PATCH_NOT_A_PATCH,
		strlen(PATCH_NOT_A_PATCH), NULL));
}

void test_patch_parse__invalid_patches_fails(void)
{
	git_patch *patch;

	cl_git_fail_with(GIT_ERROR,
		git_patch_from_buffer(&patch, PATCH_CORRUPT_GIT_HEADER,
		strlen(PATCH_CORRUPT_GIT_HEADER), NULL));
	cl_git_fail_with(GIT_ERROR,
		git_patch_from_buffer(&patch,
		PATCH_CORRUPT_MISSING_NEW_FILE,
		strlen(PATCH_CORRUPT_MISSING_NEW_FILE), NULL));
	cl_git_fail_with(GIT_ERROR,
		git_patch_from_buffer(&patch,
		PATCH_CORRUPT_MISSING_OLD_FILE,
		strlen(PATCH_CORRUPT_MISSING_OLD_FILE), NULL));
	cl_git_fail_with(GIT_ERROR,
		git_patch_from_buffer(&patch, PATCH_CORRUPT_NO_CHANGES,
		strlen(PATCH_CORRUPT_NO_CHANGES), NULL));
	cl_git_fail_with(GIT_ERROR,
		git_patch_from_buffer(&patch,
		PATCH_CORRUPT_MISSING_HUNK_HEADER,
		strlen(PATCH_CORRUPT_MISSING_HUNK_HEADER), NULL));
}

void test_patch_parse__no_newline_at_end_of_new_file(void)
{
	ensure_identical_patch_inout(PATCH_APPEND_NO_NL);
}

void test_patch_parse__no_newline_at_end_of_old_file(void)
{
	ensure_identical_patch_inout(PATCH_APPEND_NO_NL_IN_OLD_FILE);
}

void test_patch_parse__files_with_whitespaces_succeeds(void)
{
	ensure_identical_patch_inout(PATCH_NAME_WHITESPACE);
}

void test_patch_parse__filenames_with_whitespace_are_valid(void)
{
	git_patch *patch;
	const char *content = PATCH_NAME_WHITESPACE;

	cl_git_pass(git_patch_from_buffer(&patch, content, strlen(content), NULL));
	ensure_patch_validity_general(patch, "file with spaces.txt", "9432026", "83759c0");
	git_patch_free(patch);
}

void test_patch_parse__filenames_with_trailing_whitespace_are_valid(void)
{
	git_patch *patch;
	const char *content = PATCH_NAME_WHITESPACE_TRAILING;

	cl_git_pass(git_patch_from_buffer(&patch, content, strlen(content), NULL));
	ensure_patch_validity_general(patch, "file with spaces.txt ", "9432026", "83759c0");
	git_patch_free(patch);
}

void test_patch_parse__filenames_with_trailing_whitespace_and_crlf_endings_are_valid(void)
{
	git_patch *patch;
	const char *content = PATCH_NAME_WHITESPACE_TRAILING_CRLF;

	cl_git_pass(git_patch_from_buffer(&patch, content, strlen(content), NULL));
	ensure_patch_validity_general(patch, "file with spaces.txt ", "9432026", "83759c0");
	git_patch_free(patch);
}

void test_patch_parse__added_files_are_valid(void)
{
	git_patch *patch;
	const char *content = PATCH_ADD_ORIGINAL;

	cl_git_pass(git_patch_from_buffer(&patch, content, strlen(content), NULL));
	ensure_patch_validity_general(patch, "file.txt", "", "9432026");
	git_patch_free(patch);
}

void test_patch_parse__added_filenames_with_whitespace_are_valid(void)
{
	git_patch *patch;
	const char *content = PATCH_ORIGINAL_NEW_FILE_WITH_SPACE;

	cl_git_pass(git_patch_from_buffer(&patch, content, strlen(content), NULL));
	ensure_patch_validity_general(patch, "sp ace.txt", "", "789819226");
	git_patch_free(patch);
}

void test_patch_parse__deleted_files_are_valid(void)
{
	git_patch *patch;
	const char *content = PATCH_DELETE_ORIGINAL;

	cl_git_pass(git_patch_from_buffer(&patch, content, strlen(content), NULL));
	ensure_patch_validity_general(patch, "file.txt", "9432026", "");
	git_patch_free(patch);
}

void test_patch_parse__deleted_filenames_with_whitespace_are_valid(void)
{
	git_patch *patch;
	const char *content = PATCH_DELETE_ORIGINAL_SPACES;

	cl_git_pass(git_patch_from_buffer(&patch, content, strlen(content), NULL));
	ensure_patch_validity_general(patch, "file with spaces.txt", "9432026", "");
	git_patch_free(patch);
}

void test_patch_parse__lifetime_of_patch_does_not_depend_on_buffer(void)
{
	git_buf diff = GIT_BUF_INIT, rendered = GIT_BUF_INIT;
	git_patch *patch;

	cl_git_pass(git_buf_sets(&diff, PATCH_ORIGINAL_TO_CHANGE_MIDDLE));
	cl_git_pass(git_patch_from_buffer(&patch, diff.ptr, diff.size, NULL));
	git_buf_dispose(&diff);

	cl_git_pass(git_patch_to_buf(&rendered, patch));
	cl_assert_equal_s(PATCH_ORIGINAL_TO_CHANGE_MIDDLE, rendered.ptr);
	git_buf_dispose(&rendered);

	cl_git_pass(git_patch_to_buf(&rendered, patch));
	cl_assert_equal_s(PATCH_ORIGINAL_TO_CHANGE_MIDDLE, rendered.ptr);
	git_buf_dispose(&rendered);

	git_patch_free(patch);
}

void test_patch_parse__binary_file_with_missing_paths(void)
{
	git_patch *patch;
	cl_git_fail(git_patch_from_buffer(&patch, PATCH_BINARY_FILE_WITH_MISSING_PATHS,
					  strlen(PATCH_BINARY_FILE_WITH_MISSING_PATHS), NULL));
}

void test_patch_parse__binary_file_with_whitespace_paths(void)
{
	git_patch *patch;
	cl_git_fail(git_patch_from_buffer(&patch, PATCH_BINARY_FILE_WITH_WHITESPACE_PATHS,
					  strlen(PATCH_BINARY_FILE_WITH_WHITESPACE_PATHS), NULL));
}

void test_patch_parse__binary_file_with_empty_quoted_paths(void)
{
	git_patch *patch;
	cl_git_fail(git_patch_from_buffer(&patch, PATCH_BINARY_FILE_WITH_QUOTED_EMPTY_PATHS,
					  strlen(PATCH_BINARY_FILE_WITH_QUOTED_EMPTY_PATHS), NULL));
}

void test_patch_parse__binary_file_path_with_spaces(void)
{
	git_patch *patch;
	cl_git_fail(git_patch_from_buffer(&patch, PATCH_BINARY_FILE_PATH_WITH_SPACES,
					  strlen(PATCH_BINARY_FILE_PATH_WITH_SPACES), NULL));
}

void test_patch_parse__binary_file_path_without_body_paths(void)
{
	git_patch *patch;
	cl_git_fail(git_patch_from_buffer(&patch, PATCH_BINARY_FILE_PATH_WITHOUT_BODY_PATHS,
					  strlen(PATCH_BINARY_FILE_PATH_WITHOUT_BODY_PATHS), NULL));
}

void test_patch_parse__binary_file_with_truncated_delta(void)
{
	git_patch *patch;
	cl_git_fail(git_patch_from_buffer(&patch, PATCH_BINARY_FILE_WITH_TRUNCATED_DELTA,
					  strlen(PATCH_BINARY_FILE_WITH_TRUNCATED_DELTA), NULL));
	cl_assert_equal_s(git_error_last()->message, "truncated binary data at line 5");
}

void test_patch_parse__memory_leak_on_multiple_paths(void)
{
	git_patch *patch;
	cl_git_fail(git_patch_from_buffer(&patch, PATCH_MULTIPLE_OLD_PATHS, strlen(PATCH_MULTIPLE_OLD_PATHS), NULL));
}

void test_patch_parse__truncated_no_newline_at_end_of_file(void)
{
	size_t len = strlen(PATCH_APPEND_NO_NL) - strlen("at end of file\n");
	const git_diff_line *line;
	git_patch *patch;

	cl_git_pass(git_patch_from_buffer(&patch, PATCH_APPEND_NO_NL, len, NULL));
	cl_git_pass(git_patch_get_line_in_hunk(&line, patch, 0, 4));
	cl_assert_equal_s(line->content, "\\ No newline ");

	git_patch_free(patch);
}

void test_patch_parse__line_number_overflow(void)
{
	git_patch *patch;
	cl_git_fail(git_patch_from_buffer(&patch, PATCH_INTMAX_NEW_LINES, strlen(PATCH_INTMAX_NEW_LINES), NULL));
	git_patch_free(patch);
}
