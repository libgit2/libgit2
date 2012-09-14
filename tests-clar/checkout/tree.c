#include "clar_libgit2.h"

#include "git2/checkout.h"
#include "repository.h"

static git_repository *g_repo;

void test_checkout_tree__initialize(void)
{
	g_repo = cl_git_sandbox_init("testrepo");
}

void test_checkout_tree__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

void test_checkout_tree__cannot_checkout_a_non_treeish(void)
{
	git_oid oid;
	git_blob *blob;

	cl_git_pass(git_oid_fromstr(&oid, "a71586c1dfe8a71c6cbf6c129f404c5642ff31bd"));
	cl_git_pass(git_blob_lookup(&blob, g_repo, &oid));

	cl_git_fail(git_checkout_tree(g_repo, (git_object *)blob, NULL, NULL));

	git_blob_free(blob);
}
