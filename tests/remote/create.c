#include "clar_libgit2.h"

static git_repository *_repo;
static git_config *_config;
static char url[] = "http://github.com/libgit2/libgit2.git";

void test_remote_create__initialize(void)
{
	cl_fixture_sandbox("testrepo.git");

	cl_git_pass(git_repository_open(&_repo, "testrepo.git"));

	cl_git_pass(git_repository_config(&_config, _repo));
}

void test_remote_create__cleanup(void)
{
	git_config_free(_config);

	git_repository_free(_repo);
	_repo = NULL;

	cl_fixture_cleanup("testrepo.git");
}

void test_remote_create__manual(void)
{
	git_remote *remote;
	cl_git_pass(git_config_set_string(_config, "remote.origin.fetch", "+refs/heads/*:refs/remotes/origin/*"));
	cl_git_pass(git_config_set_string(_config, "remote.origin.url", url));

	cl_git_pass(git_remote_lookup(&remote, _repo, "origin"));
	cl_assert_equal_s(git_remote_name(remote), "origin");
	cl_assert_equal_s(git_remote_url(remote), url);

	git_remote_free(remote);
}
