#include "clar_libgit2.h"
#include "refs.h"

static git_repository *g_repo;

void test_refs_cmp__initialize(void)
{
	g_repo = cl_git_sandbox_init("testrepo2");
}

void test_refs_cmp__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

void test_refs_cmp__symbolic(void)
{
	git_reference *one, *two;

	cl_git_pass(git_reference_lookup(&one, g_repo, "refs/heads/symbolic-one"));
	cl_git_pass(git_reference_lookup(&two, g_repo, "refs/heads/symbolic-two"));

	cl_assert(git_reference_cmp(one, two) != 0);

	git_reference_free(one);
	git_reference_free(two);
}
