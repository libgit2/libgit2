#include "clar_libgit2.h"
#include "posix.h"
#include "blob.h"
#include "filter.h"
#include "git2/sys/filter.h"
#include "git2/sys/repository.h"
#include "custom_helpers.h"

#ifdef GIT_WIN32
# define NEWLINE "\r\n"
#else
# define NEWLINE "\n"
#endif

static char workdir_data[] =
	"some simple" NEWLINE
	"data" NEWLINE
	"that represents" NEWLINE
	"the working directory" NEWLINE
	"(smudged) contents" NEWLINE;

static char repo_data[] =
	"elpmis emos" NEWLINE
	"atad" NEWLINE
	"stneserper taht" NEWLINE
	"yrotcerid gnikrow eht" NEWLINE
	"stnetnoc )degdums(" NEWLINE;

static git_repository *g_repo = NULL;

extern int git_exec_filter_register(void);

void test_filter_exec__initialize(void)
{
	git_str reverse_cmd = GIT_STR_INIT;
	g_repo = cl_git_sandbox_init("empty_standard_repo");

	git_exec_filter_register();

	cl_git_pass(git_str_printf(&reverse_cmd, "%s/reverse %%f", cl_fixture("filters")));

	cl_git_mkfile(
		"empty_standard_repo/.gitattributes",
		"*.txt filter=bitflip -text\n"
		"*.bad1 filter=undefined -text\n"
		"*.bad2 filter=notfound -text\n");

	cl_repo_set_string(g_repo, "filter.bitflip.smudge", reverse_cmd.ptr);
	cl_repo_set_string(g_repo, "filter.bitflip.clean", reverse_cmd.ptr);

	cl_repo_set_string(g_repo, "filter.notfound.smudge", "/non/existent/path %f");
	cl_repo_set_string(g_repo, "filter.notfound.clean", "/non/existent/path %f");

	git_str_dispose(&reverse_cmd);
}

void test_filter_exec__cleanup(void)
{
	cl_git_sandbox_cleanup();
	g_repo = NULL;
}

void test_filter_exec__to_odb(void)
{
	git_filter_list *fl;
	git_buf out = GIT_BUF_INIT;
	const char *in;
	size_t in_len;

	cl_git_pass(git_filter_list_load(
		&fl, g_repo, NULL, "file.txt", GIT_FILTER_TO_ODB, 0));

	in = workdir_data;
	in_len = strlen(workdir_data);

	cl_git_pass(git_filter_list_apply_to_buffer(&out, fl, in, in_len));
	cl_assert_equal_s(repo_data, out.ptr);

	git_filter_list_free(fl);
	git_buf_dispose(&out);
}

void test_filter_exec__to_workdir(void)
{
	git_filter_list *fl;
	git_buf out = GIT_BUF_INIT;
	const char *in;
	size_t in_len;

	cl_git_pass(git_filter_list_load(
		&fl, g_repo, NULL, "file.txt", GIT_FILTER_TO_WORKTREE, 0));

	in = repo_data;
	in_len = strlen(repo_data);

	cl_git_pass(git_filter_list_apply_to_buffer(&out, fl, in, in_len));
	cl_assert_equal_s(workdir_data, out.ptr);

	git_filter_list_free(fl);
	git_buf_dispose(&out);
}

void test_filter_exec__undefined(void)
{
	git_filter_list *fl;
	git_buf out = GIT_BUF_INIT;
	const char *in;
	size_t in_len;

	cl_git_pass(git_filter_list_load(
		&fl, g_repo, NULL, "file.bad1", GIT_FILTER_TO_WORKTREE, 0));

	in = workdir_data;
	in_len = strlen(workdir_data);

	cl_git_pass(git_filter_list_apply_to_buffer(&out, fl, in, in_len));
	cl_assert_equal_s(workdir_data, out.ptr);

	git_filter_list_free(fl);
	git_buf_dispose(&out);
}

void test_filter_exec__notfound(void)
{
	git_filter_list *fl;
	git_buf out = GIT_BUF_INIT;
	const char *in;
	size_t in_len;

	cl_git_pass(git_filter_list_load(
		&fl, g_repo, NULL, "file.bad2", GIT_FILTER_TO_WORKTREE, 0));

	in = workdir_data;
	in_len = strlen(workdir_data);

	cl_git_fail(git_filter_list_apply_to_buffer(&out, fl, in, in_len));

	git_filter_list_free(fl);
	git_buf_dispose(&out);
}
