#include "clar_libgit2.h"

#include "repository.h"
#include "fetchhead.h"
#include "fetchhead_data.h"
#include "git2/clone.h"

CL_IN_CATEGORY("network")

#define LIVE_REPO_URL "git://github.com/libgit2/TestGitRepository"

static git_repository *g_repo;
static git_remote *g_origin;

void test_fetchhead_network__initialize(void)
{
	g_repo = NULL;
	cl_git_pass(git_remote_new(&g_origin, NULL, "origin", LIVE_REPO_URL, GIT_REMOTE_DEFAULT_FETCH));
}

void test_fetchhead_network__cleanup(void)
{
	g_origin = NULL;
}

static void cleanup_repository(void *path)
{
	if (g_repo) {
		git_repository_free(g_repo);
		g_repo = NULL;
	}

	cl_fixture_cleanup((const char *)path);
}


static void fetchhead_test_clone(void)
{
	cl_set_cleanup(&cleanup_repository, "./test1");

	cl_git_pass(git_clone(&g_repo, g_origin, "./test1", NULL, NULL, NULL));
}

static void fetchhead_test_fetch(const char *fetchspec, const char *expected_fetchhead)
{
	git_remote *remote;
	git_buf fetchhead_buf = GIT_BUF_INIT;
	int equals = 0;

	cl_git_pass(git_remote_load(&remote, g_repo, "origin"));
	git_remote_set_autotag(remote, GIT_REMOTE_DOWNLOAD_TAGS_AUTO);

	if(fetchspec != NULL)
		git_remote_set_fetchspec(remote, fetchspec);

	cl_git_pass(git_remote_connect(remote, GIT_DIRECTION_FETCH));
	cl_git_pass(git_remote_download(remote, NULL, NULL));
	cl_git_pass(git_remote_update_tips(remote));
	git_remote_disconnect(remote);
	git_remote_free(remote);

	cl_git_pass(git_futils_readbuffer(&fetchhead_buf,
		"./test1/.git/FETCH_HEAD"));

	equals = (strcmp(fetchhead_buf.ptr, expected_fetchhead) == 0);

	git_buf_free(&fetchhead_buf);

	cl_assert(equals);
}

void test_fetchhead_network__wildcard_spec(void)
{
	fetchhead_test_clone();
	fetchhead_test_fetch(NULL, FETCH_HEAD_WILDCARD_DATA);
}

void test_fetchhead_network__explicit_spec(void)
{
	fetchhead_test_clone();
	fetchhead_test_fetch("refs/heads/first-merge:refs/remotes/origin/first-merge", FETCH_HEAD_EXPLICIT_DATA);
}

void test_fetchhead_network__no_merges(void)
{
	git_config *config;

	fetchhead_test_clone();

	cl_git_pass(git_repository_config(&config, g_repo));
	cl_git_pass(git_config_set_string(config, "branch.master.remote", NULL));
	cl_git_pass(git_config_set_string(config, "branch.master.merge", NULL));
    git_config_free(config);

	fetchhead_test_fetch(NULL, FETCH_HEAD_NO_MERGE_DATA);
}
