#include "clar_libgit2.h"
#include "git2/sys/repository.h"

#include "apply.h"
#include "repository.h"
#include "buf_text.h"

#include "apply_common.h"

static git_repository *repo = NULL;

void test_apply_fromdiff__initialize(void)
{
	repo = cl_git_sandbox_init("renames");
}

void test_apply_fromdiff__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

static int apply_buf(
	const char *old,
	const char *oldname,
	const char *new,
	const char *newname,
	const char *patch_expected,
	const git_diff_options *diff_opts)
{
	git_patch *patch;
	git_buf result = GIT_BUF_INIT;
	git_buf patchbuf = GIT_BUF_INIT;
	char *filename;
	unsigned int mode;
	int error;

	cl_git_pass(git_patch_from_buffers(&patch,
		old, old ? strlen(old) : 0, oldname,
		new, new ? strlen(new) : 0, newname,
		diff_opts));
	cl_git_pass(git_patch_to_buf(&patchbuf, patch));

	cl_assert_equal_s(patch_expected, patchbuf.ptr);

	error = git_apply__patch(&result, &filename, &mode, old, old ? strlen(old) : 0, patch);

	if (error == 0 && new == NULL) {
		cl_assert_equal_i(0, result.size);
		cl_assert_equal_p(NULL, filename);
		cl_assert_equal_i(0, mode);
	} else {
		cl_assert_equal_s(new, result.ptr);
		cl_assert_equal_s("file.txt", filename);
		cl_assert_equal_i(0100644, mode);
	}

	git__free(filename);
	git_buf_free(&result);
	git_buf_free(&patchbuf);
	git_patch_free(patch);

	return error;
}

void test_apply_fromdiff__change_middle(void)
{
	cl_git_pass(apply_buf(
		FILE_ORIGINAL, "file.txt",
		FILE_CHANGE_MIDDLE, "file.txt",
		PATCH_ORIGINAL_TO_CHANGE_MIDDLE, NULL));
}

void test_apply_fromdiff__change_middle_nocontext(void)
{
	git_diff_options diff_opts = GIT_DIFF_OPTIONS_INIT;
	diff_opts.context_lines = 0;

	cl_git_pass(apply_buf(
		FILE_ORIGINAL, "file.txt",
		FILE_CHANGE_MIDDLE, "file.txt",
		PATCH_ORIGINAL_TO_CHANGE_MIDDLE_NOCONTEXT, &diff_opts));
}

void test_apply_fromdiff__change_firstline(void)
{
	cl_git_pass(apply_buf(
		FILE_ORIGINAL, "file.txt",
		FILE_CHANGE_FIRSTLINE, "file.txt",
		PATCH_ORIGINAL_TO_CHANGE_FIRSTLINE, NULL));
}

void test_apply_fromdiff__lastline(void)
{
	cl_git_pass(apply_buf(
		FILE_ORIGINAL, "file.txt",
		FILE_CHANGE_LASTLINE, "file.txt",
		PATCH_ORIGINAL_TO_CHANGE_LASTLINE, NULL));
}

void test_apply_fromdiff__prepend(void)
{
	cl_git_pass(apply_buf(
		FILE_ORIGINAL, "file.txt",
		FILE_PREPEND, "file.txt",
		PATCH_ORIGINAL_TO_PREPEND, NULL));
}

void test_apply_fromdiff__prepend_nocontext(void)
{
	git_diff_options diff_opts = GIT_DIFF_OPTIONS_INIT;
	diff_opts.context_lines = 0;

	cl_git_pass(apply_buf(
		FILE_ORIGINAL, "file.txt",
		FILE_PREPEND, "file.txt",
		PATCH_ORIGINAL_TO_PREPEND_NOCONTEXT, &diff_opts));
}

void test_apply_fromdiff__append(void)
{
	cl_git_pass(apply_buf(
		FILE_ORIGINAL, "file.txt",
		FILE_APPEND, "file.txt",
		PATCH_ORIGINAL_TO_APPEND, NULL));
}

void test_apply_fromdiff__append_nocontext(void)
{
	git_diff_options diff_opts = GIT_DIFF_OPTIONS_INIT;
	diff_opts.context_lines = 0;

	cl_git_pass(apply_buf(
		FILE_ORIGINAL, "file.txt",
		FILE_APPEND, "file.txt",
		PATCH_ORIGINAL_TO_APPEND_NOCONTEXT, &diff_opts));
}

void test_apply_fromdiff__prepend_and_append(void)
{
	cl_git_pass(apply_buf(
		FILE_ORIGINAL, "file.txt",
		FILE_PREPEND_AND_APPEND, "file.txt",
		PATCH_ORIGINAL_TO_PREPEND_AND_APPEND, NULL));
}

void test_apply_fromdiff__to_empty_file(void)
{
	cl_git_pass(apply_buf(
		FILE_ORIGINAL, "file.txt",
		"", NULL,
		PATCH_ORIGINAL_TO_EMPTY_FILE, NULL));
}

void test_apply_fromdiff__from_empty_file(void)
{
	cl_git_pass(apply_buf(
		"", NULL,
		FILE_ORIGINAL, "file.txt",
		PATCH_EMPTY_FILE_TO_ORIGINAL, NULL));
}

void test_apply_fromdiff__add(void)
{
	cl_git_pass(apply_buf(
		NULL, NULL,
		FILE_ORIGINAL, "file.txt",
		PATCH_ADD_ORIGINAL, NULL));
}

void test_apply_fromdiff__delete(void)
{
	cl_git_pass(apply_buf(
		FILE_ORIGINAL, "file.txt",
		NULL, NULL,
		PATCH_DELETE_ORIGINAL, NULL));
}
