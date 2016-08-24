#include "clar_libgit2.h"

static git_repository *g_repo;

void test_checkout_nullopts__initialize(void)
{
	g_repo = cl_git_sandbox_init("testrepo");
}

void test_checkout_nullopts__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

void test_checkout_nullopts__test_checkout_tree(void)
{
    cl_git_pass(git_checkout_tree(g_repo, NULL, NULL));
}

