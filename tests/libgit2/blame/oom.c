#include "blame_helpers.h"
#include "git2/sys/alloc.h"

/* Override the default allocator with one that fails a new array. */

static git_allocator std_alloc;
static git_allocator oom_alloc;
static size_t allocs_left;

static bool oom_fail(void)
{
	return allocs_left && --allocs_left == 0;
}

/* The first growth of a `git_array_t(size_t)`, like `blame->line_index` */
static void *oom_realloc(void *p, size_t n, const char *file, int line)
{
	if (!p && n == 8 * sizeof(size_t) && oom_fail())
		return NULL;

	return std_alloc.grealloc(p, n, file, line);
}

static git_repository *g_repo;

static size_t hunk_lines(git_blame *blame)
{
	size_t i, lines = 0;

	for (i = 0; i < git_blame_hunkcount(blame); i++)
		lines += git_blame_hunk_byindex(blame, i)->lines_in_hunk;

	return lines;
}

void test_blame_oom__initialize(void)
{
	git_stdalloc_init_allocator(&std_alloc);
	git_stdalloc_init_allocator(&oom_alloc);

	oom_alloc.grealloc = oom_realloc;

	cl_git_pass(git_repository_open(&g_repo, cl_fixture("testrepo/.gitted")));
}

void test_blame_oom__cleanup(void)
{
	cl_git_pass(git_allocator_setup(NULL));
	git_repository_free(g_repo);
}

/*
 * Fail each allocation in turn: the blame must either fail or be
 * complete, never succeed with a bogus line count.
 */
void test_blame_oom__file(void)
{
	git_blame *blame;
	size_t n, lines;
	int error;

	/* Load the repository's refdb and odb before failing allocations. */
	cl_git_pass(git_blame_file(&blame, g_repo, "branch_file.txt", NULL));
	cl_assert_equal_sz(2, hunk_lines(blame));
	git_blame_free(blame);

	for (n = 1; n < 1000; n++) {
		blame = NULL;
		allocs_left = n;

		cl_git_pass(git_allocator_setup(&oom_alloc));
		error = git_blame_file(&blame, g_repo, "branch_file.txt", NULL);
		lines = error ? 0 : hunk_lines(blame);
		cl_git_pass(git_allocator_setup(NULL));

		if (error) {
			cl_assert(error < 0);
		} else {
			cl_assert_equal_sz(2, lines);
		}

		git_blame_free(blame);
	}
}
