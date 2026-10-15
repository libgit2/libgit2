#include "clar_libgit2.h"

/*
 * An allocator that fails the Nth allocation, so that every allocation of an
 * operation can be made to fail in turn.
 */

static git_allocator std_alloc;
static git_allocator failing_alloc;
static size_t alloc_count;
static size_t fail_at;

static void *failing_malloc(size_t n, const char *file, int line)
{
	return (alloc_count++ == fail_at) ? NULL : std_alloc.gmalloc(n, file, line);
}

static void *failing_realloc(void *p, size_t n, const char *file, int line)
{
	return (alloc_count++ == fail_at) ? NULL : std_alloc.grealloc(p, n, file, line);
}

void test_refs_oom__initialize(void)
{
	git_stdalloc_init_allocator(&std_alloc);
	git_stdalloc_init_allocator(&failing_alloc);

	failing_alloc.gmalloc = failing_malloc;
	failing_alloc.grealloc = failing_realloc;
}

void test_refs_oom__cleanup(void)
{
	cl_git_pass(git_allocator_setup(NULL));
}

void test_refs_oom__lookup_fails_or_finds_head(void)
{
	git_repository *repo;
	git_reference *head;
	int error;

	/*
	 * Fail each allocation in turn until looking up HEAD in a newly
	 * opened repository, which opens the refdb, needs no more of them:
	 * the lookup either reports the failure or finds HEAD.
	 */
	for (fail_at = 0; ; fail_at++) {
		alloc_count = 0;
		head = NULL;

		cl_git_pass(git_repository_open(&repo, cl_fixture("testrepo.git")));

		cl_git_pass(git_allocator_setup(&failing_alloc));
		error = git_reference_lookup(&head, repo, "HEAD");
		cl_git_pass(git_allocator_setup(NULL));

		if (error == 0)
			cl_assert_equal_s("refs/heads/master", git_reference_symbolic_target(head));

		git_reference_free(head);
		git_repository_free(repo);

		if (alloc_count <= fail_at) {
			/* no allocation failed, so the lookup must have succeeded */
			cl_git_pass(error);
			break;
		}
	}
}
