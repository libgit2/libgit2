#include "clar_libgit2.h"
#include "buffer.h"
#include "refs.h"
#include "posix.h"

static git_repository *_repo;
static git_buf _path;
static char *_actual;

void test_repo_message__initialize(void)
{
        _repo = cl_git_sandbox_init("testrepo.git");
}

void test_repo_message__cleanup(void)
{
        cl_git_sandbox_cleanup();
	git_buf_free(&_path);
	git__free(_actual);
	_actual = NULL;
}

void test_repo_message__none(void)
{
	cl_assert_equal_i(GIT_ENOTFOUND, git_repository_message(NULL, 0, _repo));
}

void test_repo_message__message(void)
{
	const char expected[] = "Test\n\nThis is a test of the emergency broadcast system\n";
	ssize_t len;

	cl_git_pass(git_buf_joinpath(&_path, git_repository_path(_repo), "MERGE_MSG"));
	cl_git_mkfile(git_buf_cstr(&_path), expected);

	len = git_repository_message(NULL, 0, _repo);
	cl_assert(len > 0);
	_actual = git__malloc(len + 1);
	cl_assert(_actual != NULL);

	cl_assert(git_repository_message(_actual, len, _repo) > 0);
	_actual[len] = '\0';
	cl_assert_equal_s(expected, _actual);

	cl_git_pass(p_unlink(git_buf_cstr(&_path)));
	cl_assert_equal_i(GIT_ENOTFOUND, git_repository_message(NULL, 0, _repo));
}
