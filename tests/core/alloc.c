#include "clar_libgit2.h"
#include "clar_libgit2_alloc.h"
#include "alloc.h"
#include "global.h"

void test_core_alloc__initialize(void)
{
	/*
	 * This here is probably not quite obvious. If executing
	 * libgit2_clar -score::alloc, then the allocation tests
	 * are the first to get executed. Thus, we do not yet
	 * have encountered any errors yet, and thus there is no
	 * global state allocated yet.
	 *
	 * Now for the funny thing: if the first error that we
	 * encounter is an out-of-memory error, then we call
	 * `git_error_set_oom`. This again calls `GIT_GLOBAL`,
	 * which requests the current global state. If there is
	 * none, then we try to allocate one. Guess what? We're
	 * out of memory, so this fails and we call
	 * `git_error_set_oom`. Ad infinitum, until we crash
	 * because of recursion.
	 *
	 * This is why we just explicitly request the global
	 * state now.
	 */
	GIT_GLOBAL;
}

void test_core_alloc__cleanup(void)
{
	cl_alloc_reset();
}

void test_core_alloc__oom(void)
{
	void *ptr = NULL;

	cl_alloc_limit(0);

	cl_assert(git__malloc(1) == NULL);
	cl_assert(git__calloc(1, 1) == NULL);
	cl_assert(git__realloc(ptr, 1) == NULL);
	cl_assert(git__strdup("test") == NULL);
	cl_assert(git__strndup("test", 4) == NULL);
}

void test_core_alloc__single_byte_is_exhausted(void)
{
	void *ptr;

	cl_alloc_limit(1);

	cl_assert(ptr = git__malloc(1));
	cl_assert(git__malloc(1) == NULL);
	git__free(ptr);
}

void test_core_alloc__free_replenishes_byte(void)
{
	void *ptr;

	cl_alloc_limit(1);

	cl_assert(ptr = git__malloc(1));
	cl_assert(git__malloc(1) == NULL);
	git__free(ptr);
	cl_assert(ptr = git__malloc(1));
	git__free(ptr);
}

void test_core_alloc__realloc(void)
{
	char *ptr = NULL;

	cl_alloc_limit(3);

	cl_assert(ptr = git__realloc(ptr, 1));
	*ptr = 'x';

	cl_assert(ptr = git__realloc(ptr, 1));
	cl_assert_equal_i(*ptr, 'x');

	cl_assert(ptr = git__realloc(ptr, 2));
	cl_assert_equal_i(*ptr, 'x');

	cl_assert(git__realloc(ptr, 2) == NULL);

	cl_assert(ptr = git__realloc(ptr, 1));
	cl_assert_equal_i(*ptr, 'x');

	git__free(ptr);
}
