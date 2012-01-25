#include "clar_libgit2.h"
#include "posix.h"

void test_repo_open__bare_empty_repo(void)
{
	git_repository *repo;

	cl_git_pass(git_repository_open(&repo, cl_fixture("empty_bare.git")));
	cl_assert(git_repository_path(repo) != NULL);
	cl_assert(git_repository_workdir(repo) == NULL);

	git_repository_free(repo);
}

void test_repo_open__standard_empty_repo(void)
{
	git_repository *repo;

	cl_git_pass(git_repository_open(&repo, cl_fixture("empty_standard_repo/.gitted")));
	cl_assert(git_repository_path(repo) != NULL);
	cl_assert(git_repository_workdir(repo) != NULL);

	git_repository_free(repo);
}
