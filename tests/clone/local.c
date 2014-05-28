#include "clar_libgit2.h"

#include "git2/clone.h"
#include "clone.h"
#include "buffer.h"

void assert_clone(const char *path, git_clone_local_t opt, int val)
{
	cl_assert_equal_i(val, git_clone__should_clone_local(path, opt));
}

void test_clone_local__should_clone_local(void)
{
	git_buf buf = GIT_BUF_INIT;
	const char *path;

	/* we use a fixture path because it needs to exist for us to want to clone */
	
	cl_git_pass(git_buf_printf(&buf, "file://%s", cl_fixture("testrepo.git")));
	cl_assert_equal_i(false, git_clone__should_clone_local(buf.ptr, GIT_CLONE_LOCAL_AUTO));
	cl_assert_equal_i(true,  git_clone__should_clone_local(buf.ptr, GIT_CLONE_LOCAL));
	cl_assert_equal_i(true,  git_clone__should_clone_local(buf.ptr, GIT_CLONE_LOCAL_NO_LINKS));
	cl_assert_equal_i(false, git_clone__should_clone_local(buf.ptr, GIT_CLONE_NO_LOCAL));
	git_buf_free(&buf);

	path = cl_fixture("testrepo.git");
	cl_assert_equal_i(true,  git_clone__should_clone_local(path, GIT_CLONE_LOCAL_AUTO));
	cl_assert_equal_i(true,  git_clone__should_clone_local(path, GIT_CLONE_LOCAL));
	cl_assert_equal_i(true,  git_clone__should_clone_local(path, GIT_CLONE_LOCAL_NO_LINKS));
	cl_assert_equal_i(false, git_clone__should_clone_local(path, GIT_CLONE_NO_LOCAL));
}
