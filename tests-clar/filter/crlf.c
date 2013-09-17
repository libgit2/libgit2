#include "clar_libgit2.h"
#include "git2/sys/filter.h"

static git_repository *g_repo = NULL;

void test_filter_crlf__initialize(void)
{
	g_repo = cl_git_sandbox_init("crlf");

	cl_git_mkfile("crlf/.gitattributes",
		"*.txt text\n*.bin binary\n*.crlf text eol=crlf\n*.lf text eol=lf\n");

	cl_repo_set_bool(g_repo, "core.autocrlf", true);
}

void test_filter_crlf__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

void test_filter_crlf__to_worktree(void)
{
	git_filter_list *fl;
	git_filter *crlf;
	git_buf in = { 0 }, out = { 0 };

	cl_git_pass(git_filter_list_new(&fl, g_repo, GIT_FILTER_TO_WORKTREE));

	crlf = git_filter_lookup(GIT_FILTER_CRLF);
	cl_assert(crlf != NULL);

	cl_git_pass(git_filter_list_push(fl, crlf, NULL));

	in.ptr = "Some text\nRight here\n";
	in.size = strlen(in.ptr);

	cl_git_pass(git_filter_list_apply_to_data(&out, fl, &in));

#ifdef GIT_WIN32
	cl_assert_equal_s("Some text\r\nRight here\r\n", out.ptr);
#else
	cl_assert_equal_s("Some text\nRight here\n", out.ptr);
#endif

	git_filter_list_free(fl);
	git_buf_free(&out);
}

void test_filter_crlf__to_odb(void)
{
	git_filter_list *fl;
	git_filter *crlf;
	git_buf in = { 0 }, out = { 0 };

	cl_git_pass(git_filter_list_new(&fl, g_repo, GIT_FILTER_TO_ODB));

	crlf = git_filter_lookup(GIT_FILTER_CRLF);
	cl_assert(crlf != NULL);

	cl_git_pass(git_filter_list_push(fl, crlf, NULL));

	in.ptr = "Some text\r\nRight here\r\n";
	in.size = strlen(in.ptr);

	cl_git_pass(git_filter_list_apply_to_data(&out, fl, &in));

	cl_assert_equal_s("Some text\nRight here\n", out.ptr);

	git_filter_list_free(fl);
	git_buf_free(&out);
}
