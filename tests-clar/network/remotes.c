#include "clar_libgit2.h"
#include "buffer.h"
#include "refspec.h"

static git_remote *_remote;
static git_repository *_repo;
static const git_refspec *_refspec;

void test_network_remotes__initialize(void)
{
	cl_fixture_sandbox("testrepo.git");

	cl_git_pass(git_repository_open(&_repo, "testrepo.git"));
	cl_git_pass(git_remote_load(&_remote, _repo, "test"));

	_refspec = git_remote_fetchspec(_remote);
	cl_assert(_refspec != NULL);
}

void test_network_remotes__cleanup(void)
{
	git_remote_free(_remote);
	git_repository_free(_repo);
	cl_fixture_cleanup("testrepo.git");
}

void test_network_remotes__parsing(void)
{
	cl_assert(!strcmp(git_remote_name(_remote), "test"));
	cl_assert(!strcmp(git_remote_url(_remote), "git://github.com/libgit2/libgit2"));
}

void test_network_remotes__refspec_parsing(void)
{
	cl_assert(!strcmp(git_refspec_src(_refspec), "refs/heads/*"));
	cl_assert(!strcmp(git_refspec_dst(_refspec), "refs/remotes/test/*"));
}

void test_network_remotes__fnmatch(void)
{
	cl_git_pass(git_refspec_src_match(_refspec, "refs/heads/master"));
	cl_git_pass(git_refspec_src_match(_refspec, "refs/heads/multi/level/branch"));
}

void test_network_remotes__transform(void)
{
	char ref[1024];

	memset(ref, 0x0, sizeof(ref));
	cl_git_pass(git_refspec_transform(ref, sizeof(ref), _refspec, "refs/heads/master"));
	cl_assert(!strcmp(ref, "refs/remotes/test/master"));
}

void test_network_remotes__transform_r(void)
{
	git_buf buf = GIT_BUF_INIT;

	cl_git_pass(git_refspec_transform_r(&buf,  _refspec, "refs/heads/master"));
	cl_assert(!strcmp(git_buf_cstr(&buf), "refs/remotes/test/master"));
	git_buf_free(&buf);
}
