#include "clar_libgit2.h"

static git_repository *g_repo;

void test_refs_special__initialize(void)
{
	g_repo = cl_git_sandbox_init("testrepo");
}

void test_refs_special__cleanup(void)
{
	cl_git_sandbox_cleanup();
}
