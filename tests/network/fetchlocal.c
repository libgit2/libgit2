#include "clar_libgit2.h"

#include "buffer.h"
#include "path.h"
#include "remote.h"

static int transfer_cb(const git_transfer_progress *stats, void *payload)
{
	int *callcount = (int*)payload;
	GIT_UNUSED(stats);
	(*callcount)++;
	return 0;
}

static void cleanup_local_repo(void *path)
{
	cl_fixture_cleanup((char *)path);
}

void test_network_fetchlocal__complete(void)
{
	git_repository *repo;
	git_remote *origin;
	int callcount = 0;
	git_strarray refnames = {0};

	const char *url = cl_git_fixture_url("testrepo.git");
	git_remote_callbacks callbacks = GIT_REMOTE_CALLBACKS_INIT;

	callbacks.transfer_progress = transfer_cb;
	callbacks.payload = &callcount;

	cl_set_cleanup(&cleanup_local_repo, "foo");
	cl_git_pass(git_repository_init(&repo, "foo", true));

	cl_git_pass(git_remote_create(&origin, repo, GIT_REMOTE_ORIGIN, url));
	git_remote_set_callbacks(origin, &callbacks);
	cl_git_pass(git_remote_connect(origin, GIT_DIRECTION_FETCH));
	cl_git_pass(git_remote_download(origin));
	cl_git_pass(git_remote_update_tips(origin, NULL, NULL));

	cl_git_pass(git_reference_list(&refnames, repo));
	cl_assert_equal_i(19, (int)refnames.count);
	cl_assert(callcount > 0);

	git_strarray_free(&refnames);
	git_remote_free(origin);
	git_repository_free(repo);
}

static void cleanup_sandbox(void *unused)
{
	GIT_UNUSED(unused);
	cl_git_sandbox_cleanup();
}

void test_network_fetchlocal__partial(void)
{
	git_repository *repo = cl_git_sandbox_init("partial-testrepo");
	git_remote *origin;
	int callcount = 0;
	git_strarray refnames = {0};
	const char *url;
	git_remote_callbacks callbacks = GIT_REMOTE_CALLBACKS_INIT;

	callbacks.transfer_progress = transfer_cb;
	callbacks.payload = &callcount;

	cl_set_cleanup(&cleanup_sandbox, NULL);
	cl_git_pass(git_reference_list(&refnames, repo));
	cl_assert_equal_i(1, (int)refnames.count);

	url = cl_git_fixture_url("testrepo.git");
	cl_git_pass(git_remote_create(&origin, repo, GIT_REMOTE_ORIGIN, url));
	git_remote_set_callbacks(origin, &callbacks);
	cl_git_pass(git_remote_connect(origin, GIT_DIRECTION_FETCH));
	cl_git_pass(git_remote_download(origin));
	cl_git_pass(git_remote_update_tips(origin, NULL, NULL));

	git_strarray_free(&refnames);

	cl_git_pass(git_reference_list(&refnames, repo));
	cl_assert_equal_i(20, (int)refnames.count); /* 18 remote + 1 local */
	cl_assert(callcount > 0);

	git_strarray_free(&refnames);
	git_remote_free(origin);
}
