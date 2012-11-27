#include "clar_libgit2.h"
#include "refs.h"

static git_repository *g_repo;

void test_refs_lookup__initialize(void)
{
	g_repo = cl_git_sandbox_init("testrepo.git");
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
	git_reference_free(b);

	cl_git_pass(git_reference_lookup_resolved(&b, g_repo, "HEAD_TRACKER", 5));
	cl_assert(git_reference_cmp(a, b) == 0);
	git_reference_free(b);

	git_reference_free(a);
}

void test_refs_lookup__oid(void)
{
	git_oid tag, expected;

	cl_git_pass(git_reference_name_to_id(&tag, g_repo, "refs/tags/point_to_blob"));
	cl_git_pass(git_oid_fromstr(&expected, "1385f264afb75a56a5bec74243be9b367ba4ca08"));
	cl_assert(git_oid_cmp(&tag, &expected) == 0);
}
