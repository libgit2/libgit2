#include "clar_libgit2.h"

void test_repo_getters__initialize(void)
{
	cl_fixture_sandbox("testrepo.git");
}

void test_repo_getters__cleanup(void)
{
	cl_fixture_cleanup("testrepo.git");
}

void test_repo_getters__empty(void)
{
	git_repository *repo_empty, *repo_normal;

	cl_git_pass(git_repository_open(&repo_normal, cl_fixture("testrepo.git")));
	cl_assert(git_repository_is_empty(repo_normal) == 0);
	git_repository_free(repo_normal);

	cl_git_pass(git_repository_open(&repo_empty, cl_fixture("empty_bare.git")));
	cl_assert(git_repository_is_empty(repo_empty) == 1);
	git_repository_free(repo_empty);
}

void test_repo_getters__retrieving_the_odb_honors_the_refcount(void)
{
	git_odb *odb;
	git_repository *repo;

	cl_git_pass(git_repository_open(&repo, "testrepo.git"));

	cl_git_pass(git_repository_odb(&odb, repo));
	cl_assert(((git_refcount *)odb)->refcount == 2);

	git_repository_free(repo);
	cl_assert(((git_refcount *)odb)->refcount == 1);

	git_odb_free(odb);
}
