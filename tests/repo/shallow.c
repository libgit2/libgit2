#include "clar_libgit2.h"
#include "fileops.h"

static git_repository *g_repo;
static git_oid g_shallow_oid;

void test_repo_shallow__initialize(void)
{
	cl_git_pass(git_oid_fromstr(&g_shallow_oid, "be3563ae3f795b2b4353bcce3a527ad0a4f7f644"));
}

void test_repo_shallow__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

void test_repo_shallow__no_shallow_file(void)
{
	g_repo = cl_git_sandbox_init("testrepo.git");
	cl_assert_equal_i(0, git_repository_is_shallow(g_repo));
}

void test_repo_shallow__empty_shallow_file(void)
{
	g_repo = cl_git_sandbox_init("testrepo.git");
	cl_git_mkfile("testrepo.git/shallow", "");
	cl_assert_equal_i(0, git_repository_is_shallow(g_repo));
}

void test_repo_shallow__shallow_repo(void)
{
	g_repo = cl_git_sandbox_init("shallow.git");
	cl_assert_equal_i(1, git_repository_is_shallow(g_repo));
}

void test_repo_shallow__clears_errors(void)
{
	g_repo = cl_git_sandbox_init("testrepo.git");
	cl_assert_equal_i(0, git_repository_is_shallow(g_repo));
	cl_assert_equal_p(NULL, giterr_last());
}

void test_repo_shallow__shallow_oids(void)
{
	git_oidarray oids, oids2;
	g_repo = cl_git_sandbox_init("shallow.git");

	cl_git_pass(git_repository_shallow_roots(&oids, g_repo));
	cl_assert_equal_i(1, oids.count);
	cl_assert_equal_oid(&g_shallow_oid, &oids.ids[0]);

	cl_git_pass(git_repository_shallow_roots(&oids2, g_repo));
	cl_assert_equal_p(oids.ids, oids2.ids);
}
