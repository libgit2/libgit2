#include "clar_libgit2.h"
#include "buffer.h"
#include "refs.h"
#include "posix.h"

static git_repository *_repo;
static git_buf _path;
static char *_actual;

void test_repo_state__initialize(void)
{
	_repo = cl_git_sandbox_init("testrepo.git");
}

void test_repo_state__cleanup(void)
{
	cl_git_sandbox_cleanup();
	_repo = NULL;
	git_buf_free(&_path);
	git__free(_actual);
	_actual = NULL;
}

void test_repo_state__none(void)
{
	/* The repo should be at its default state */
	cl_assert_equal_i(GIT_REPOSITORY_STATE_NONE, git_repository_state(_repo));
}

void test_repo_state__merge(void)
{
	/* Then it should recognise that .git/MERGE_HEAD and friends mean their respective states */
	cl_git_pass(git_buf_joinpath(&_path, git_repository_path(_repo), GIT_MERGE_HEAD_FILE));
	cl_git_mkfile(git_buf_cstr(&_path), "dummy");
	cl_assert_equal_i(GIT_REPOSITORY_STATE_MERGE, git_repository_state(_repo));
}

void test_repo_state__revert(void)
{
	const char expected[] = "Revert \"a fourth commit\"\n\n"
		"This reverts commit 9fd738e8f7967c078dceed8190330fc8648ee56a.\n";
	ssize_t len;

	cl_git_pass(git_buf_joinpath(&_path, git_repository_path(_repo), GIT_REVERT_HEAD_FILE));
	cl_git_mkfile(git_buf_cstr(&_path), "9fd738e8f7967c078dceed8190330fc8648ee56a\n");
	cl_assert_equal_i(GIT_REPOSITORY_STATE_REVERT, git_repository_state(_repo));

	len = git_repository_message(NULL, 0, _repo);
	cl_assert(len > 0);
	_actual = git__malloc(len + 1);
	cl_assert(_actual != NULL);

	cl_assert(git_repository_message(_actual, len, _repo) > 0);
	_actual[len] = '\0';
	cl_assert_equal_s(expected, _actual);
}

void test_repo_state__cherry_pick(void)
{
	const char expected[] = "Test\n\nThis is a test of the emergency broadcast system\n";
	ssize_t len;

	cl_git_pass(git_buf_joinpath(&_path, git_repository_path(_repo), GIT_CHERRY_PICK_HEAD_FILE));
	cl_git_mkfile(git_buf_cstr(&_path), "dummy");
	cl_git_pass(git_buf_joinpath(&_path, git_repository_path(_repo), "COMMIT_EDITMSG"));
	cl_git_mkfile(git_buf_cstr(&_path), expected);
	cl_assert_equal_i(GIT_REPOSITORY_STATE_CHERRY_PICK, git_repository_state(_repo));

	len = git_repository_message(NULL, 0, _repo);
	cl_assert(len > 0);
	_actual = git__malloc(len + 1);
	cl_assert(_actual != NULL);

	cl_assert(git_repository_message(_actual, len, _repo) > 0);
	_actual[len] = '\0';
	cl_assert_equal_s(expected, _actual);
}
