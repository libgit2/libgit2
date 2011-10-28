#include "clay_libgit2.h"
#include "vector.h"

/* initial size of 1 would cause writing past array bounds */
void test_core_vector__0(void)
{
	git_vector x;
	int i;
	git_vector_init(&x, 1, NULL);
	for (i = 0; i < 10; ++i) {
		git_vector_insert(&x, (void*) 0xabc);
	}
	git_vector_free(&x);
}


/* don't read past array bounds on remove() */
void test_core_vector__1(void)
{
	git_vector x;
	// make initial capacity exact for our insertions.
	git_vector_init(&x, 3, NULL);
	git_vector_insert(&x, (void*) 0xabc);
	git_vector_insert(&x, (void*) 0xdef);
	git_vector_insert(&x, (void*) 0x123);

	git_vector_remove(&x, 0); // used to read past array bounds.
	git_vector_free(&x);
}


static int test_cmp(const void *a, const void *b)
{
	return *(const int *)a - *(const int *)b;
}

/* remove duplicates */
void test_core_vector__2(void)
{
	git_vector x;
	int *ptrs[2];

	ptrs[0] = git__malloc(sizeof(int));
	ptrs[1] = git__malloc(sizeof(int));

	*ptrs[0] = 2;
	*ptrs[1] = 1;

	cl_git_pass(git_vector_init(&x, 5, test_cmp));
	cl_git_pass(git_vector_insert(&x, ptrs[0]));
	cl_git_pass(git_vector_insert(&x, ptrs[1]));
	cl_git_pass(git_vector_insert(&x, ptrs[1]));
	cl_git_pass(git_vector_insert(&x, ptrs[0]));
	cl_git_pass(git_vector_insert(&x, ptrs[1]));
	cl_assert(x.length == 5);

	git_vector_uniq(&x);
	cl_assert(x.length == 2);

	git_vector_free(&x);

	git__free(ptrs[0]);
	git__free(ptrs[1]);
}


