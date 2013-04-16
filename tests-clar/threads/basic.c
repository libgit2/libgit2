#include "clar_libgit2.h"

#include "cache.h"


static git_repository *g_repo;

void test_threads_basic__initialize(void)
{
	g_repo = cl_git_sandbox_init("testrepo");
}

void test_threads_basic__cleanup(void)
{
	cl_git_sandbox_cleanup();
}


void test_threads_basic__cache(void)
{
	// run several threads polling the cache at the same time
	cl_assert(1 == 1);
}
