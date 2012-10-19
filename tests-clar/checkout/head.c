#include "clar_libgit2.h"
#include "refs.h"

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
	git_reference *head;

	cl_git_pass(git_reference_create_symbolic(&head, g_repo, GIT_HEAD_FILE, "refs/heads/hide/and/seek", 1));
	git_reference_free(head);

	cl_assert_equal_i(GIT_EORPHANEDHEAD, git_checkout_head(g_repo, NULL, NULL));
}
