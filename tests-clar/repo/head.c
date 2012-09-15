#include "clar_libgit2.h"
#include "refs.h"

git_repository *repo;

void test_repo_head__initialize(void)
{
	repo = cl_git_sandbox_init("testrepo.git");
}

void test_repo_head__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

void test_repo_head__head_detached(void)
{
	git_reference *ref;
	git_oid oid;

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
}

void test_repo_head__head_orphan(void)
{
	git_reference *ref;

	cl_assert(git_repository_head_orphan(repo) == 0);

	/* orphan HEAD */
	cl_git_pass(git_reference_create_symbolic(&ref, repo, "HEAD", "refs/heads/orphan", 1));
	cl_assert(git_repository_head_orphan(repo) == 1);
	git_reference_free(ref);

	/* take the reop back to it's original state */
	cl_git_pass(git_reference_create_symbolic(&ref, repo, "HEAD", "refs/heads/master", 1));
	cl_assert(git_repository_head_orphan(repo) == 0);

	git_reference_free(ref);
}

static void assert_head_is_correctly_detached(void)
{
	git_reference *head;
	git_object *commit;

	cl_assert_equal_i(true, git_repository_head_detached(repo));

	cl_git_pass(git_repository_head(&head, repo));

	cl_git_pass(git_object_lookup(&commit, repo, git_reference_oid(head), GIT_OBJ_COMMIT));

	git_object_free(commit);
	git_reference_free(head);
}

void test_repo_head__detach_head_Detaches_HEAD_and_make_it_point_to_the_peeled_commit(void)
{
	cl_assert_equal_i(false, git_repository_head_detached(repo));

	cl_git_pass(git_repository_detach_head(repo));

	assert_head_is_correctly_detached();
}

void test_repo_head__detach_head_Fails_if_HEAD_and_point_to_a_non_commitish(void)
{
	git_reference *head;

	cl_git_pass(git_reference_create_symbolic(&head, repo, GIT_HEAD_FILE, "refs/tags/point_to_blob", 1));

	cl_git_fail(git_repository_detach_head(repo));

	git_reference_free(head);
}
