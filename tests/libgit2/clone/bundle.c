#include "clar_libgit2.h"

#include "git2/clone.h"

void test_clone_bundle__v2(void)
{
	git_repository *repo;

	cl_git_pass(git_clone(
	        &repo, cl_fixture("bundle/testrepo.bundle"), "./clone.git",
	        NULL));

	git_repository_free(repo);
}
