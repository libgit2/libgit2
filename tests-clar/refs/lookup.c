#include "clar_libgit2.h"

static git_repository *g_repo;

void test_refs_lookup__initialize(void)
{
	g_repo = cl_git_sandbox_init("status");
}

void test_refs_lookup__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

void test_refs_lookup__with_resolve(void)
{
	git_reference *a, *b, *temp;

	cl_git_pass(git_reference_lookup(&temp, g_repo, "HEAD"));
	cl_git_pass(git_reference_resolve(&a, temp));
	git_reference_free(temp);

	cl_git_pass(git_reference_lookup_resolved(&b, g_repo, "HEAD", 5));

	cl_assert(git_reference_cmp(a, b) == 0);

	git_reference_free(a);
	git_reference_free(b);
}
