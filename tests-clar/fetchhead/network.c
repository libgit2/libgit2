#include "clar_libgit2.h"

#include "repository.h"
#include "fetchhead.h"
#include "fetchhead_data.h"

CL_IN_CATEGORY("network")

#define LIVE_REPO_URL "git://github.com/libgit2/TestGitRepository"

static git_repository *g_repo;

void test_fetchhead_network__initialize(void)
{
	g_repo = NULL;
}

static void cleanup_repository(void *path)
{
	if (g_repo)
		git_repository_free(g_repo);
	cl_fixture_cleanup((const char *)path);
}


void test_fetchhead_network__network_full(void)
{
	git_remote *remote;
	git_off_t bytes;
	git_indexer_stats stats;
	git_buf fetchhead_buf = GIT_BUF_INIT;
	int equals = 0;

	cl_set_cleanup(&cleanup_repository, "./test1");

	cl_git_pass(git_repository_init(&g_repo, "./test1", 0));
	cl_git_pass(git_remote_add(&remote, g_repo, "origin", LIVE_REPO_URL));
	git_remote_set_autotag(remote, GIT_REMOTE_DOWNLOAD_TAGS_AUTO);

	cl_git_pass(git_remote_connect(remote, GIT_DIR_FETCH));
	cl_git_pass(git_remote_download(remote, &bytes, &stats));
	git_remote_disconnect(remote);

	cl_git_pass(git_remote_update_tips(remote));
    git_remote_free(remote);

	cl_git_pass(git_futils_readbuffer(&fetchhead_buf,
		"./test1/.git/FETCH_HEAD"));

	equals = (strcmp(fetchhead_buf.ptr, FETCH_HEAD_DATA) == 0);

	git_buf_free(&fetchhead_buf);

	cl_assert(equals);
}

