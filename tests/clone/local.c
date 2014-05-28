#include "clar_libgit2.h"

#include "git2/clone.h"
#include "clone.h"
#include "buffer.h"
#include "path.h"
#include "posix.h"

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

void test_clone_local__hardlinks(void)
{
	git_repository *repo;
	git_remote *remote;
	git_signature *sig;
	git_buf buf = GIT_BUF_INIT;
	struct stat st;

	cl_git_pass(git_repository_init(&repo, "./clone.git", true));
	cl_git_pass(git_remote_create(&remote, repo, "origin", cl_fixture("testrepo.git")));
	cl_git_pass(git_signature_now(&sig, "foo", "bar"));
	cl_git_pass(git_clone_local_into(repo, remote, NULL, NULL, true, sig));

	git_remote_free(remote);
	git_repository_free(repo);

	/*
	 * We can't rely on the link option taking effect in the first
	 * clone, since the temp dir and fixtures dir may reside on
	 * different filesystems. We perform the second clone
	 * side-by-side to make sure this is the case.
	 */

	cl_git_pass(git_repository_init(&repo, "./clone2.git", true));
	cl_git_pass(git_buf_puts(&buf, cl_git_path_url("clone.git")));
	cl_git_pass(git_remote_create(&remote, repo, "origin", buf.ptr));
	cl_git_pass(git_clone_local_into(repo, remote, NULL, NULL, true, sig));

#ifndef GIT_WIN32
	git_buf_clear(&buf);
	cl_git_pass(git_buf_join_n(&buf, '/', 4, git_repository_path(repo), "objects", "08", "b041783f40edfe12bb406c9c9a8a040177c125"));

	cl_git_pass(p_stat(buf.ptr, &st));
	cl_assert(st.st_nlink > 1);
#endif

	git_remote_free(remote);
	git_repository_free(repo);
	git_buf_clear(&buf);

	cl_git_pass(git_repository_init(&repo, "./clone3.git", true));
	cl_git_pass(git_buf_puts(&buf, cl_git_path_url("clone.git")));
	cl_git_pass(git_remote_create(&remote, repo, "origin", buf.ptr));
	cl_git_pass(git_clone_local_into(repo, remote, NULL, NULL, false, sig));

	git_buf_clear(&buf);
	cl_git_pass(git_buf_join_n(&buf, '/', 4, git_repository_path(repo), "objects", "08", "b041783f40edfe12bb406c9c9a8a040177c125"));

	cl_git_pass(p_stat(buf.ptr, &st));
	cl_assert_equal_i(1, st.st_nlink);

	git_buf_free(&buf);
	git_signature_free(sig);
	git_remote_free(remote);
	git_repository_free(repo);
}
