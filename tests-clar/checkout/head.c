#include "clar_libgit2.h"
#include "refs.h"
#include "repo/repo_helpers.h"

static git_repository *g_repo;

void test_checkout_head__initialize(void)
{
	g_repo = cl_git_sandbox_init("testrepo");
}

void test_checkout_head__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

void test_checkout_head__checking_out_an_orphaned_head_returns_GIT_EORPHANEDHEAD(void)
{
	make_head_orphaned(g_repo, NON_EXISTING_HEAD);

	cl_assert_equal_i(GIT_EORPHANEDHEAD, git_checkout_head(g_repo, NULL, NULL));
}
