#include "clay_libgit2.h"

#define REPOSITORY_FOLDER "testrepo.git"

static git_remote *remote;
static git_repository *repo;
static git_config *cfg;
static const git_refspec *refspec;

void test_network_remotes__initialize(void)
{
	cl_fixture_sandbox(REPOSITORY_FOLDER);
	cl_git_pass(git_repository_open(&repo, REPOSITORY_FOLDER));
	cl_git_pass(git_repository_config(&cfg, repo, NULL));
	cl_git_pass(git_remote_get(&remote, cfg, "test"));
	refspec = git_remote_fetchspec(remote);
	cl_assert(refspec != NULL);
}

void test_network_remotes__cleanup(void)
{
	git_config_free(cfg);
	git_repository_free(repo);
	git_remote_free(remote);
}

void test_network_remotes__parsing(void)
{
	cl_assert(!strcmp(git_remote_name(remote), "test"));
	cl_assert(!strcmp(git_remote_url(remote), "git://github.com/libgit2/libgit2"));
}

void test_network_remotes__refspec_parsing(void)
{
	cl_assert(!strcmp(git_refspec_src(refspec), "refs/heads/*"));
	cl_assert(!strcmp(git_refspec_dst(refspec), "refs/remotes/test/*"));
}

void test_network_remotes__fnmatch(void)
{
	cl_git_pass(git_refspec_src_match(refspec, "refs/heads/master"));
	cl_git_pass(git_refspec_src_match(refspec, "refs/heads/multi/level/branch"));
}

void test_network_remotes__transform(void)
{
	char ref[1024];

	memset(ref, 0x0, sizeof(ref));
	cl_git_pass(git_refspec_transform(ref, sizeof(ref), refspec, "refs/heads/master"));
	cl_assert(!strcmp(ref, "refs/remotes/test/master"));
}
