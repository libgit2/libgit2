#include "clar_libgit2.h"

void test_repo_getters__initialize(void)
{
	cl_fixture_sandbox("testrepo.git");
}

void test_repo_getters__cleanup(void)
{
	cl_fixture_cleanup("testrepo.git");
}

void test_repo_getters__empty(void)
{
	git_repository *repo_empty, *repo_normal;

	cl_git_pass(git_repository_open(&repo_normal, cl_fixture("testrepo.git")));
	cl_assert(git_repository_is_empty(repo_normal) == 0);
	git_repository_free(repo_normal);

	cl_git_pass(git_repository_open(&repo_empty, cl_fixture("empty_bare.git")));
	cl_assert(git_repository_is_empty(repo_empty) == 1);
	git_repository_free(repo_empty);
}

void test_repo_getters__head_detached(void)
{
	git_repository *repo;
	git_reference *ref;
	git_oid oid;

	cl_git_pass(git_repository_open(&repo, "testrepo.git"));

	cl_assert(git_repository_head_detached(repo) == 0);

	/* detach the HEAD */
	git_oid_fromstr(&oid, "c47800c7266a2be04c571c04d5a6614691ea99bd");
	cl_git_pass(git_reference_create_oid(&ref, repo, "HEAD", &oid, 1));
	cl_assert(git_repository_head_detached(repo) == 1);
	git_reference_free(ref);

	/* take the reop back to it's original state */
	cl_git_pass(git_reference_create_symbolic(&ref, repo, "HEAD", "refs/heads/master", 1));
	cl_assert(git_repository_head_detached(repo) == 0);

	git_reference_free(ref);
	git_repository_free(repo);
}

void test_repo_getters__head_orphan(void)
{
	git_repository *repo;
	git_reference *ref;

	cl_git_pass(git_repository_open(&repo, "testrepo.git"));

	cl_assert(git_repository_head_orphan(repo) == 0);

	/* orphan HEAD */
	cl_git_pass(git_reference_create_symbolic(&ref, repo, "HEAD", "refs/heads/orphan", 1));
	cl_assert(git_repository_head_orphan(repo) == 1);
	git_reference_free(ref);

	/* take the reop back to it's original state */
	cl_git_pass(git_reference_create_symbolic(&ref, repo, "HEAD", "refs/heads/master", 1));
	cl_assert(git_repository_head_orphan(repo) == 0);

	git_reference_free(ref);
	git_repository_free(repo);
}

void test_repo_getters__retrieving_the_odb_honors_the_refcount(void)
{
	git_odb *odb;
	git_repository *repo;

	cl_git_pass(git_repository_open(&repo, "testrepo.git"));

	cl_git_pass(git_repository_odb(&odb, repo));
	cl_assert(((git_refcount *)odb)->refcount == 2);

	git_repository_free(repo);
	cl_assert(((git_refcount *)odb)->refcount == 1);

	git_odb_free(odb);
}
