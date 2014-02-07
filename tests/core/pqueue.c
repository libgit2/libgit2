#include "clar_libgit2.h"
#include "pqueue.h"

static int cmp_ints(const void *v1, const void *v2)
{
	int i1 = *(int *)v1, i2 = *(int *)v2;
	return (i1 < i2) ? -1 : (i1 > i2) ? 1 : 0;
}

void test_core_pqueue__items_are_put_in_order(void)
{
	git_pqueue pq;
	int i, vals[20];

	cl_git_pass(git_pqueue_init(&pq, 0, 20, cmp_ints));

	for (i = 0; i < 20; ++i) {
		if (i < 10)
			vals[i] = 10 - i; /* 10 down to 1 */
		else
			vals[i] = i + 1;  /* 11 up to 20 */

		cl_git_pass(git_pqueue_insert(&pq, &vals[i]));
	}

	cl_assert_equal_i(20, git_pqueue_size(&pq));

	for (i = 1; i <= 20; ++i) {
		void *p = git_pqueue_pop(&pq);
		cl_assert(p);
		cl_assert_equal_i(i, *(int *)p);
	}

	cl_assert_equal_i(0, git_pqueue_size(&pq));

	git_pqueue_free(&pq);
}

void test_core_pqueue__interleave_inserts_and_pops(void)
{
	git_pqueue pq;
	int chunk, v, i, vals[200];

	cl_git_pass(git_pqueue_init(&pq, 0, 20, cmp_ints));

	for (v = 0, chunk = 20; chunk <= 200; chunk += 20) {
		/* push the next 20 */
		for (; v < chunk; ++v) {
			vals[v] = (v & 1) ? 200 - v : v;
			cl_git_pass(git_pqueue_insert(&pq, &vals[v]));
		}

		/* pop the lowest 10 */
		for (i = 0; i < 10; ++i)
			(void)git_pqueue_pop(&pq);
	}

	cl_assert_equal_i(100, git_pqueue_size(&pq));

	/* at this point, we've popped 0-99 */

	for (v = 100; v < 200; ++v) {
		void *p = git_pqueue_pop(&pq);
		cl_assert(p);
		cl_assert_equal_i(v, *(int *)p);
	}

	cl_assert_equal_i(0, git_pqueue_size(&pq));

	git_pqueue_free(&pq);
}

void test_core_pqueue__max_heap_size(void)
{
	git_pqueue pq;
	int i, vals[100];

	cl_git_pass(git_pqueue_init(&pq, GIT_PQUEUE_FIXED_SIZE, 50, cmp_ints));

	for (i = 0; i < 100; ++i) {
		vals[i] = (i & 1) ? 100 - i : i;
		cl_git_pass(git_pqueue_insert(&pq, &vals[i]));
	}

	cl_assert_equal_i(50, git_pqueue_size(&pq));

	for (i = 50; i < 100; ++i) {
		void *p = git_pqueue_pop(&pq);
		cl_assert(p);
		cl_assert_equal_i(i, *(int *)p);
	}

	cl_assert_equal_i(0, git_pqueue_size(&pq));

	git_pqueue_free(&pq);

}
