#include "clar_libgit2.h"

#include "repository.h"

static git_repository *g_repo;

void test_object_lookup__initialize(void)
{
   cl_git_pass(git_repository_open(&g_repo, cl_fixture("testrepo.git")));
}

void test_object_lookup__cleanup(void)
{
   git_repository_free(g_repo);
}

void test_object_lookup__looking_up_an_exisiting_object_by_its_wrong_type_returns_ENOTFOUND(void)
{
	const char *commit = "e90810b8df3e80c413d903f631643c716887138d";
	git_oid oid;
	git_object *object;

	cl_git_pass(git_oid_fromstr(&oid, commit));
	cl_assert_equal_i(GIT_ENOTFOUND, git_object_lookup(&object, g_repo, &oid, GIT_OBJ_TAG));
}

void test_object_lookup__looking_up_a_non_exisiting_object_returns_ENOTFOUND(void)
{
	const char *unknown = "deadbeefdeadbeefdeadbeefdeadbeefdeadbeef";
	git_oid oid;
	git_object *object;

	cl_git_pass(git_oid_fromstr(&oid, unknown));
	cl_assert_equal_i(GIT_ENOTFOUND, git_object_lookup(&object, g_repo, &oid, GIT_OBJ_ANY));
}
