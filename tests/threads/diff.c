#include "clar_libgit2.h"
#include "thread-utils.h"

static git_repository *g_repo;
static git_tree *a, *b;
static git_atomic counts[4];

void test_threads_diff__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

static void run_in_parallel(
	int repeats, int threads, void *(*func)(void *),
	void (*before_test)(void), void (*after_test)(void))
{
	int r, t, *id = git__calloc(threads, sizeof(int));
#ifdef GIT_THREADS
	git_thread *th = git__calloc(threads, sizeof(git_thread));
#else
	void *th = NULL;
#endif

	cl_assert(id != NULL && th != NULL);

	for (r = 0; r < repeats; ++r) {
		g_repo = cl_git_sandbox_reopen(); /* reopen sandbox to flush caches */

		if (before_test) before_test();

		for (t = 0; t < threads; ++t) {
			id[t] = t;
#ifdef GIT_THREADS
			cl_git_pass(git_thread_create(&th[t], NULL, func, &id[t]));
#else
			cl_assert(func(&id[t]) == &id[t]);
#endif
		}

#ifdef GIT_THREADS
		for (t = 0; t < threads; ++t)
			cl_git_pass(git_thread_join(th[t], NULL));
		memset(th, 0, threads * sizeof(git_thread));
#endif

		if (after_test) after_test();
	}

	git__free(id);
	git__free(th);
}

static void setup_trees(void)
{
	cl_git_pass(git_revparse_single(
		(git_object **)&a, g_repo, "0017bd4ab1^{tree}"));
	cl_git_pass(git_revparse_single(
		(git_object **)&b, g_repo, "26a125ee1b^{tree}"));

	memset(counts, 0, sizeof(counts));
}

#define THREADS 20

static void free_trees(void)
{
	git_tree_free(a); a = NULL;
	git_tree_free(b); b = NULL;

	cl_assert_equal_i(288, git_atomic_get(&counts[0]));
	cl_assert_equal_i(112, git_atomic_get(&counts[1]));
	cl_assert_equal_i( 80, git_atomic_get(&counts[2]));
	cl_assert_equal_i( 96, git_atomic_get(&counts[3]));
}

static void *run_index_diffs(void *arg)
{
	int thread = *(int *)arg;
	git_diff_options opts = GIT_DIFF_OPTIONS_INIT;
	git_diff *diff = NULL;
	size_t i;
	int exp[4] = { 0, 0, 0, 0 };

//	fprintf(stderr, "%d >>>\n", thread);

	switch (thread & 0x03) {
	case 0: /* diff index to workdir */;
		cl_git_pass(git_diff_index_to_workdir(&diff, g_repo, NULL, &opts));
		break;
	case 1: /* diff tree 'a' to index */;
		cl_git_pass(git_diff_tree_to_index(&diff, g_repo, a, NULL, &opts));
		break;
	case 2: /* diff tree 'b' to index */;
		cl_git_pass(git_diff_tree_to_index(&diff, g_repo, b, NULL, &opts));
		break;
	case 3: /* diff index to workdir (explicit index) */;
		{
			git_index *idx;
			cl_git_pass(git_repository_index(&idx, g_repo));
			cl_git_pass(git_diff_index_to_workdir(&diff, g_repo, idx, &opts));
			git_index_free(idx);
			break;
		}
	}

//	fprintf(stderr, "%d <<<\n", thread);

	/* keep some diff stats to make sure results are as expected */

	i = git_diff_num_deltas(diff);
	git_atomic_add(&counts[0], (int32_t)i);
	exp[0] = (int)i;

	while (i > 0) {
		switch (git_diff_get_delta(diff, --i)->status) {
		case GIT_DELTA_MODIFIED: exp[1]++; git_atomic_inc(&counts[1]); break;
		case GIT_DELTA_ADDED:    exp[2]++; git_atomic_inc(&counts[2]); break;
		case GIT_DELTA_DELETED:  exp[3]++; git_atomic_inc(&counts[3]); break;
		default: break;
		}
	}

//	fprintf(stderr, "%2d: [%d] total %d (M %d A %d D %d)\n",
//			thread, (int)(thread & 0x03), exp[0], exp[1], exp[2], exp[3]);

	switch (thread & 0x03) {
	case 0: case 3:
		cl_assert_equal_i(8, exp[0]); cl_assert_equal_i(4, exp[1]);
		cl_assert_equal_i(0, exp[2]); cl_assert_equal_i(4, exp[3]);
		break;
	case 1:
		cl_assert_equal_i(12, exp[0]); cl_assert_equal_i(3, exp[1]);
		cl_assert_equal_i(7, exp[2]); cl_assert_equal_i(2, exp[3]);
		break;
	case 2:
		cl_assert_equal_i(8, exp[0]); cl_assert_equal_i(3, exp[1]);
		cl_assert_equal_i(3, exp[2]); cl_assert_equal_i(2, exp[3]);
		break;
	}

	git_diff_free(diff);

	return arg;
}

void test_threads_diff__concurrent_diffs(void)
{
	g_repo = cl_git_sandbox_init("status");

	run_in_parallel(
		20, 32, run_index_diffs, setup_trees, free_trees);
}
