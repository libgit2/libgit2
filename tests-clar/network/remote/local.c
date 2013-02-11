#include "clar_libgit2.h"
#include "buffer.h"
#include "path.h"
#include "posix.h"

static git_repository *repo;
static git_buf file_path_buf = GIT_BUF_INIT;
static git_remote *remote;

void test_network_remote_local__initialize(void)
{
	cl_git_pass(git_repository_init(&repo, "remotelocal/", 0));
	cl_assert(repo != NULL);
}

void test_network_remote_local__cleanup(void)
{
	git_buf_free(&file_path_buf);

	git_remote_free(remote);
	remote = NULL;

	git_repository_free(repo);
	repo = NULL;

	cl_fixture_cleanup("remotelocal");
}

static int count_ref__cb(git_remote_head *head, void *payload)
{
	int *count = (int *)payload;

	(void)head;
	(*count)++;

	return 0;
}

static int ensure_peeled__cb(git_remote_head *head, void *payload)
{
	GIT_UNUSED(payload);

	if(strcmp(head->name, "refs/tags/test^{}") != 0)
		return 0;

	return git_oid_streq(&head->oid, "e90810b8df3e80c413d903f631643c716887138d");
}

static void connect_to_local_repository(const char *local_repository)
{
	git_buf_sets(&file_path_buf, cl_git_path_url(local_repository));

	cl_git_pass(git_remote_create_inmemory(&remote, repo, NULL, git_buf_cstr(&file_path_buf)));
	cl_git_pass(git_remote_connect(remote, GIT_DIRECTION_FETCH));

}

void test_network_remote_local__connected(void)
{
	connect_to_local_repository(cl_fixture("testrepo.git"));
	cl_assert(git_remote_connected(remote));

	git_remote_disconnect(remote);
	cl_assert(!git_remote_connected(remote));
}

void test_network_remote_local__retrieve_advertised_references(void)
{
	int how_many_refs = 0;

	connect_to_local_repository(cl_fixture("testrepo.git"));

	cl_git_pass(git_remote_ls(remote, &count_ref__cb, &how_many_refs));

	cl_assert_equal_i(how_many_refs, 26);
}

void test_network_remote_local__retrieve_advertised_references_from_spaced_repository(void)
{
	int how_many_refs = 0;

	cl_fixture_sandbox("testrepo.git");
	cl_git_pass(p_rename("testrepo.git", "spaced testrepo.git"));

	connect_to_local_repository("spaced testrepo.git");

	cl_git_pass(git_remote_ls(remote, &count_ref__cb, &how_many_refs));

	cl_assert_equal_i(how_many_refs, 26);

	git_remote_free(remote);	/* Disconnect from the "spaced repo" before the cleanup */
	remote = NULL;

	cl_fixture_cleanup("spaced testrepo.git");
}

void test_network_remote_local__nested_tags_are_completely_peeled(void)
{
	connect_to_local_repository(cl_fixture("testrepo.git"));

	cl_git_pass(git_remote_ls(remote, &ensure_peeled__cb, NULL));
}
