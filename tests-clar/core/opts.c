#include "clar_libgit2.h"
#include "cache.h"

void test_core_opts__readwrite(void)
{
	size_t old_val = 0;
	size_t new_val = 0;

	git_libgit2_opts(GIT_OPT_GET_MWINDOW_SIZE, &old_val);
	git_libgit2_opts(GIT_OPT_SET_MWINDOW_SIZE, (size_t)1234);
	git_libgit2_opts(GIT_OPT_GET_MWINDOW_SIZE, &new_val);

	cl_assert(new_val == 1234);

	git_libgit2_opts(GIT_OPT_SET_MWINDOW_SIZE, old_val);
	git_libgit2_opts(GIT_OPT_GET_MWINDOW_SIZE, &new_val);

	cl_assert(new_val == old_val);

	git_libgit2_opts(GIT_OPT_GET_ODB_CACHE_SIZE, &old_val);

	cl_assert(old_val == GIT_DEFAULT_CACHE_SIZE);

	git_libgit2_opts(GIT_OPT_SET_ODB_CACHE_SIZE, (size_t)GIT_DEFAULT_CACHE_SIZE*2);
	git_libgit2_opts(GIT_OPT_GET_ODB_CACHE_SIZE, &new_val);

	cl_assert(new_val == (GIT_DEFAULT_CACHE_SIZE*2));

	git_libgit2_opts(GIT_OPT_GET_ODB_CACHE_SIZE, &old_val);
}
