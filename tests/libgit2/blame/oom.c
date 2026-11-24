#include "blame_helpers.h"

/*
 * An allocator that fails the Nth allocation, so that every allocation of an
 * operation can be made to fail in turn.
 */

static git_allocator std_alloc;
static git_allocator failing_alloc;
static size_t alloc_count;
static size_t fail_at;

static git_repository *g_repo;
static git_blame *g_fileblame;

static void *failing_malloc(size_t n, const char *file, int line)
{
	return (alloc_count++ == fail_at) ? NULL : std_alloc.gmalloc(n, file, line);
}

static void *failing_realloc(void *p, size_t n, const char *file, int line)
{
	return (alloc_count++ == fail_at) ? NULL : std_alloc.grealloc(p, n, file, line);
}

void test_blame_oom__initialize(void)
{
	git_stdalloc_init_allocator(&std_alloc);
	git_stdalloc_init_allocator(&failing_alloc);

	failing_alloc.gmalloc = failing_malloc;
	failing_alloc.grealloc = failing_realloc;

	cl_git_pass(git_repository_open(&g_repo, cl_fixture("blametest.git")));
	cl_git_pass(git_blame_file(&g_fileblame, g_repo, "b.txt", NULL));
}

void test_blame_oom__cleanup(void)
{
	cl_git_pass(git_allocator_setup(NULL));
	git_blame_free(g_fileblame);
	git_repository_free(g_repo);
}

void test_blame_oom__buffer_fails_or_has_every_hunk(void)
{
	const char *buffer = "\
EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n\
EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n\
xEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n\
EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
	git_blame *expected, *blame;
	int error;

	cl_git_pass(git_blame_buffer(&expected, g_fileblame, buffer, strlen(buffer)));

	/*
	 * Fail each allocation in turn until the blame needs no more of them:
	 * it either reports the failure or matches the blame made without one,
	 * never a blame that leaves out the buffer's changes.
	 */
	for (fail_at = 0; ; fail_at++) {
		alloc_count = 0;
		blame = NULL;

		cl_git_pass(git_allocator_setup(&failing_alloc));
		error = git_blame_buffer(&blame, g_fileblame, buffer, strlen(buffer));
		cl_git_pass(git_allocator_setup(NULL));

		if (error == 0)
			cl_assert_equal_i(git_blame_hunkcount(expected), git_blame_hunkcount(blame));

		git_blame_free(blame);

		if (alloc_count <= fail_at) {
			/* no allocation failed, so the blame must have succeeded */
			cl_git_pass(error);
			break;
		}
	}

	git_blame_free(expected);
}
