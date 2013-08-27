#include "clar_libgit2.h"
#include "crlf.h"

static git_repository *g_repo = NULL;

void test_filter_blob__initialize(void)
{
	g_repo = cl_git_sandbox_init("crlf");
	cl_git_mkfile("crlf/.gitattributes",
		"*.txt text\n*.bin binary\n*.crlf text eol=crlf\n*.lf text eol=lf\n");
}

void test_filter_blob__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

void test_filter_blob__all_crlf(void)
{
	git_blob *blob;
	git_buffer buf = GIT_BUFFER_INIT;

	cl_git_pass(git_revparse_single(
		(git_object **)&blob, g_repo, "a9a2e891")); /* all-crlf */

	cl_assert_equal_s(ALL_CRLF_TEXT_RAW, git_blob_rawcontent(blob));

	cl_git_pass(git_blob_filtered_content(&buf, blob, "file.bin", 1));

	cl_assert_equal_s(ALL_CRLF_TEXT_RAW, buf.ptr);

	cl_git_pass(git_blob_filtered_content(&buf, blob, "file.crlf", 1));

	/* in this case, raw content has crlf in it already */
	cl_assert_equal_s(ALL_CRLF_TEXT_AS_CRLF, buf.ptr);

	cl_git_pass(git_blob_filtered_content(&buf, blob, "file.lf", 1));

	cl_assert_equal_s(ALL_CRLF_TEXT_AS_LF, buf.ptr);

	git_buffer_free(&buf);
	git_blob_free(blob);
}
