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

void test_diff_oom__initialize(void)
{
	git_stdalloc_init_allocator(&std_alloc);
	git_stdalloc_init_allocator(&failing_alloc);

	failing_alloc.gmalloc = failing_malloc;
	failing_alloc.grealloc = failing_realloc;
}

void test_diff_oom__cleanup(void)
{
	cl_git_pass(git_allocator_setup(NULL));
}

void test_diff_oom__patch_from_buffers_fails_or_has_the_hunk(void)
{
	const char *a = "one\ntwo\nthree\n";
	const char *b = "one\n2\nthree\n";
	git_patch *patch;
	int error;

	/*
	 * Fail each allocation in turn until the diff needs no more of them:
	 * the diff either reports the failure or has its one hunk, never an
	 * empty patch reported as success.
	 */
	for (fail_at = 0; ; fail_at++) {
		alloc_count = 0;
		patch = NULL;

		cl_git_pass(git_allocator_setup(&failing_alloc));
		error = git_patch_from_buffers(&patch, a, strlen(a), NULL, b, strlen(b), NULL, NULL);
		cl_git_pass(git_allocator_setup(NULL));

		if (error == 0)
			cl_assert_equal_i(1, git_patch_num_hunks(patch));

		git_patch_free(patch);

		if (alloc_count <= fail_at) {
			/* no allocation failed, so the diff must have succeeded */
			cl_git_pass(error);
			break;
		}
	}
}
