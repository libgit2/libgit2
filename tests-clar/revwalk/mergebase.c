#include "clar_libgit2.h"

static git_repository *_repo;

void test_revwalk_mergebase__initialize(void)
{
	cl_git_pass(git_repository_open(&_repo, cl_fixture("testrepo.git")));
}

void test_revwalk_mergebase__cleanup(void)
{
	git_repository_free(_repo);
}

void test_revwalk_mergebase__single1(void)
{
	git_oid result, one, two, expected;

	git_oid_fromstr(&one, "c47800c7266a2be04c571c04d5a6614691ea99bd ");
	git_oid_fromstr(&two, "9fd738e8f7967c078dceed8190330fc8648ee56a");
	git_oid_fromstr(&expected, "5b5b025afb0b4c913b4c338a42934a3863bf3644");

	cl_git_pass(git_merge_base(&result, _repo, &one, &two));
	cl_assert(git_oid_cmp(&result, &expected) == 0);
}

void test_revwalk_mergebase__single2(void)
{
	git_oid result, one, two, expected;

	git_oid_fromstr(&one, "763d71aadf09a7951596c9746c024e7eece7c7af");
	git_oid_fromstr(&two, "a65fedf39aefe402d3bb6e24df4d4f5fe4547750");
	git_oid_fromstr(&expected, "c47800c7266a2be04c571c04d5a6614691ea99bd");

	cl_git_pass(git_merge_base(&result, _repo, &one, &two));
	cl_assert(git_oid_cmp(&result, &expected) == 0);
}
