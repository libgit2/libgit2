#include "clar_libgit2.h"
#include "futils.h"

static git_repository *repo;

void test_fetch_local__initialize(void)
{
	cl_git_pass(git_repository_init(&repo, "./fetch", 0));
}

void test_fetch_local__cleanup(void)
{
	git_repository_free(repo);
	repo = NULL;

	cl_fixture_cleanup("./fetch");
}

void test_fetch_local__defaults(void)
{
	git_remote *remote;
	git_object *obj;
	git_oid expected_id;

	cl_git_pass(git_remote_create(&remote, repo, "test",
		cl_fixture("testrepo.git")));
	cl_git_pass(git_remote_fetch(remote, NULL, NULL, NULL));

	git_oid_fromstr(&expected_id, "258f0e2a959a364e40ed6603d5d44fbb24765b10");

	cl_git_pass(git_revparse_single(&obj, repo, "refs/remotes/test/haacked"));
	cl_assert_equal_oid(&expected_id, git_object_id(obj));

	git_object_free(obj);
	git_remote_free(remote);
}
