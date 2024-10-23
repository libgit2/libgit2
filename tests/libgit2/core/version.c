#include "clar_libgit2.h"

void test_core_version__check(void)
{
#if !LIBGIT2_VERSION_CHECK(1,6,3)
	cl_fail("version check");
#endif

#if LIBGIT2_VERSION_CHECK(99,99,99)
	cl_fail("version check");
#endif
}
