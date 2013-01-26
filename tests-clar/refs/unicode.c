#include "clar_libgit2.h"

static git_repository *repo;

void test_refs_unicode__initialize(void)
{
	cl_fixture_sandbox("testrepo.git");

	cl_git_pass(git_repository_open(&repo, "testrepo.git"));
}

void test_refs_unicode__cleanup(void)
{
	git_repository_free(repo);
	repo = NULL;

	cl_fixture_cleanup("testrepo.git");
}

void test_refs_unicode__create_and_lookup(void)
{
	git_reference *ref0, *ref1, *ref2;
	git_repository *repo2;

	const char *REFNAME = "refs/heads/" "\305" "ngstr" "\366" "m";
	const char *master = "refs/heads/master";

	/* Create the reference */
	cl_git_pass(git_reference_lookup(&ref0, repo, master));
	cl_git_pass(git_reference_create(&ref1, repo, REFNAME, git_reference_target(ref0), 0));
	cl_assert_equal_s(REFNAME, git_reference_name(ref1));

	/* Lookup the reference in a different instance of the repository */
	cl_git_pass(git_repository_open(&repo2, "testrepo.git"));
	cl_git_pass(git_reference_lookup(&ref2, repo2, REFNAME));

	cl_assert(git_oid_cmp(git_reference_target(ref1), git_reference_target(ref2)) == 0);
	cl_assert_equal_s(REFNAME, git_reference_name(ref2));

	git_reference_free(ref0);
	git_reference_free(ref1);
	git_reference_free(ref2);
	git_repository_free(repo2);
}
