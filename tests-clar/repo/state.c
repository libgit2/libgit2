#include "clar_libgit2.h"
#include "buffer.h"
#include "refs.h"
#include "posix.h"

static git_repository *_repo;
static git_buf _path;

void test_repo_state__initialize(void)
{
	_repo = cl_git_sandbox_init("testrepo.git");
}

void test_repo_state__cleanup(void)
{
	cl_git_sandbox_cleanup();
	git_buf_free(&_path);
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
	cl_git_pass(git_buf_joinpath(&_path, git_repository_path(_repo), GIT_REVERT_HEAD_FILE));
	cl_git_mkfile(git_buf_cstr(&_path), "dummy");
	cl_assert_equal_i(GIT_REPOSITORY_STATE_REVERT, git_repository_state(_repo));
}

void test_repo_state__cherry_pick(void)
{
	cl_git_pass(git_buf_joinpath(&_path, git_repository_path(_repo), GIT_CHERRY_PICK_HEAD_FILE));
	cl_git_mkfile(git_buf_cstr(&_path), "dummy");
	cl_assert_equal_i(GIT_REPOSITORY_STATE_CHERRY_PICK, git_repository_state(_repo));
}
