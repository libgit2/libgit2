#include "clar_libgit2.h"

void test_core_caps__0(void)
{
	int major, minor, rev, caps;

	git_libgit2_version(&major, &minor, &rev);
	cl_assert_equal_i(LIBGIT2_VER_MAJOR, major);
	cl_assert_equal_i(LIBGIT2_VER_MINOR, minor);
	cl_assert_equal_i(LIBGIT2_VER_REVISION, rev);

	caps = git_libgit2_capabilities();

#ifdef GIT_THREADS
	cl_assert((caps & GIT_CAP_THREADS) != 0);
#else
	cl_assert((caps & GIT_CAP_THREADS) == 0);
#endif

#if defined(GIT_SSL) || defined(GIT_WINHTTP)
	cl_assert((caps & GIT_CAP_HTTPS) != 0);
#else
	cl_assert((caps & GIT_CAP_HTTPS) == 0);
#endif

#if defined(GIT_SSH)
	cl_assert((caps & GIT_CAP_SSH) != 0);
#else
	cl_assert((caps & GIT_CAP_SSH) == 0);
#endif
}
