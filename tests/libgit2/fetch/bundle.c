#include "clar_libgit2.h"

#include "git2/remote.h"

static git_repository *g_repo;

void test_fetch_bundle__initialize(void)
{
	g_repo = cl_git_sandbox_init("testrepo");
}

void test_fetch_bundle__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

void test_fetch_bundle__v2(void)
{
	git_remote *remote;
	git_oid expected_id;
	git_object *obj;

	cl_git_pass(git_remote_create(
	        &remote, g_repo, "bundle",
	        cl_fixture("bundle/testrepo_fetch.bundle")));
	cl_git_pass(git_remote_fetch(remote, NULL, NULL, NULL));
	git_oid_from_string(
	        &expected_id, "d70553b411e163b98a1b704d5bf33c5438decd9c",
	        GIT_OID_SHA1);
	cl_git_pass(
	        git_revparse_single(&obj, g_repo, "refs/remotes/bundle/master"));
	cl_assert_equal_oid(&expected_id, git_object_id(obj));

	git_object_free(obj);
	git_remote_free(remote);
}
