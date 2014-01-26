#include "clar_libgit2.h"
#include "buffer.h"
#include "refs.h"
#include "posix.h"

static git_repository *_repo;
static git_buf _path;
static git_buf _actual;

void test_repo_message__initialize(void)
{
        _repo = cl_git_sandbox_init("testrepo.git");
	git_buf_init(&_actual, 0);
}

void test_repo_message__cleanup(void)
{
        cl_git_sandbox_cleanup();
	git_buf_free(&_path);
	git_buf_free(&_actual);
}

void test_repo_message__none(void)
{
	cl_assert_equal_i(GIT_ENOTFOUND, git_repository_message(&_actual, _repo));
}

void test_repo_message__message(void)
{
	const char expected[] = "Test\n\nThis is a test of the emergency broadcast system\n";

	cl_git_pass(git_buf_joinpath(&_path, git_repository_path(_repo), "MERGE_MSG"));
	cl_git_mkfile(git_buf_cstr(&_path), expected);

	cl_git_pass(git_repository_message(&_actual, _repo));
	cl_assert_equal_s(expected, _actual);
	git_buf_free(&_actual);

	cl_git_pass(p_unlink(git_buf_cstr(&_path)));
	cl_assert_equal_i(GIT_ENOTFOUND, git_repository_message(&_actual, _repo));
}
