#include "clar_libgit2.h"

void test_core_version__query(void)
{
	int major, minor, rev;

	git_libgit2_version(&major, &minor, &rev);
	cl_assert_equal_i(LIBGIT2_VERSION_MAJOR, major);
	cl_assert_equal_i(LIBGIT2_VERSION_MINOR, minor);
	cl_assert_equal_i(LIBGIT2_VERSION_REVISION, rev);
}

void test_core_version__check(void)
{
#if !LIBGIT2_VERSION_CHECK(1,6,3)
	cl_fail("version check");
#endif

#if LIBGIT2_VERSION_CHECK(99,99,99)
	cl_fail("version check");
#endif
}
