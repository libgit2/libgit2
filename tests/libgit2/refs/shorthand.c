#include "clar_libgit2.h"

#include "repository.h"

static git_repository *repo;

void test_refs_shorthand__initialize_fs(void)
{
	cl_git_pass(git_repository_open(&repo, cl_fixture("testrepo.git")));
}

void test_refs_shorthand__initialize_reftable(void)
{
	cl_git_pass(git_repository_open(&repo, cl_fixture("testrepo-reftable.git")));
}

void test_refs_shorthand__cleanup(void)
{
	git_repository_free(repo);
}

void assert_shorthand(git_repository *repo, const char *refname, const char *shorthand)
{
	git_reference *ref;

	cl_git_pass(git_reference_lookup(&ref, repo, refname));
	cl_assert_equal_s(git_reference_shorthand(ref), shorthand);
	git_reference_free(ref);
}

void test_refs_shorthand__0(void)
{
	assert_shorthand(repo, "refs/heads/master", "master");
	assert_shorthand(repo, "refs/tags/test", "test");
	assert_shorthand(repo, "refs/remotes/test/master", "test/master");
	assert_shorthand(repo, "refs/notes/fanout", "notes/fanout");
}
