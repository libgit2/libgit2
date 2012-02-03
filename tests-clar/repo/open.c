#include "clar_libgit2.h"
#include "posix.h"

static git_repository *repo;

void test_repo_open__cleanup(void)
{
	git_repository_free(repo);
}

void test_repo_open__bare_empty_repo(void)
{
	cl_git_pass(git_repository_open(&repo, cl_fixture("empty_bare.git")));

	cl_assert(git_repository_path(repo) != NULL);
	cl_assert(git__suffixcmp(git_repository_path(repo), "/") == 0);

	cl_assert(git_repository_workdir(repo) == NULL);
}

void test_repo_open__standard_empty_repo_through_gitdir(void)
{
	cl_git_pass(git_repository_open(&repo, cl_fixture("empty_standard_repo/.gitted")));

	cl_assert(git_repository_path(repo) != NULL);
	cl_assert(git__suffixcmp(git_repository_path(repo), "/") == 0);

	cl_assert(git_repository_workdir(repo) != NULL);
	cl_assert(git__suffixcmp(git_repository_workdir(repo), "/") == 0);
}

void test_repo_open__standard_empty_repo_through_workdir(void)
{
	cl_fixture_sandbox("empty_standard_repo");
	cl_git_pass(p_rename("empty_standard_repo/.gitted", "empty_standard_repo/.git"));

	cl_git_pass(git_repository_open(&repo, "empty_standard_repo"));

	cl_assert(git_repository_path(repo) != NULL);
	cl_assert(git__suffixcmp(git_repository_path(repo), "/") == 0);

	cl_assert(git_repository_workdir(repo) != NULL);
	cl_assert(git__suffixcmp(git_repository_workdir(repo), "/") == 0);

	cl_fixture_cleanup("empty_standard_repo");
}
