#include "clar_libgit2.h"

CL_IN_CATEGORY("network")

static git_repository *_repo;
static int counter;

void test_network_fetch__initialize(void)
{
	cl_git_pass(git_repository_init(&_repo, "./fetch", 0));
}

void test_network_fetch__cleanup(void)
{
	git_repository_free(_repo);
	cl_fixture_cleanup("./fetch");
}

static int update_tips(const char *refname, const git_oid *a, const git_oid *b, void *data)
{
	refname = refname;
	a = a;
	b = b;
	data = data;

	++counter;

	return 0;
}

static void do_fetch(const char *url, int flag, int n)
{
	git_remote *remote;
	git_off_t bytes;
	git_indexer_stats stats;
	git_remote_callbacks callbacks;

	memset(&callbacks, 0, sizeof(git_remote_callbacks));
	callbacks.update_tips = update_tips;
	counter = 0;

	cl_git_pass(git_remote_add(&remote, _repo, "test", url));
	git_remote_set_callbacks(remote, &callbacks);
	git_remote_set_autotag(remote, flag);
	cl_git_pass(git_remote_connect(remote, GIT_DIR_FETCH));
	cl_git_pass(git_remote_download(remote, &bytes, &stats));
	git_remote_disconnect(remote);
	cl_git_pass(git_remote_update_tips(remote));
	cl_assert_equal_i(counter, n);

	git_remote_free(remote);
}

void test_network_fetch__default_git(void)
{
	do_fetch("git://github.com/libgit2/TestGitRepository.git", GIT_REMOTE_DOWNLOAD_TAGS_AUTO, 6);
}

void test_network_fetch__default_http(void)
{
	do_fetch("http://github.com/libgit2/TestGitRepository.git", GIT_REMOTE_DOWNLOAD_TAGS_AUTO, 6);
}

void test_network_fetch__no_tags_git(void)
{
	do_fetch("git://github.com/libgit2/TestGitRepository.git", GIT_REMOTE_DOWNLOAD_TAGS_NONE, 3);
}

void test_network_fetch__no_tags_http(void)
{
	do_fetch("http://github.com/libgit2/TestGitRepository.git", GIT_REMOTE_DOWNLOAD_TAGS_NONE, 3);
}
