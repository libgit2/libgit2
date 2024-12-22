#include "clar_libgit2.h"

void test_core_features__basic(void)
{
	int caps = git_libgit2_features();

#ifdef GIT_THREADS
	cl_assert((caps & GIT_FEATURE_THREADS) != 0);
#else
	cl_assert((caps & GIT_FEATURE_THREADS) == 0);
#endif

#ifdef GIT_HTTPS
	cl_assert((caps & GIT_FEATURE_HTTPS) != 0);
#endif

#if defined(GIT_SSH)
	cl_assert((caps & GIT_FEATURE_SSH) != 0);
#else
	cl_assert((caps & GIT_FEATURE_SSH) == 0);
#endif

#if defined(GIT_USE_NSEC)
	cl_assert((caps & GIT_FEATURE_NSEC) != 0);
#else
	cl_assert((caps & GIT_FEATURE_NSEC) == 0);
#endif
}
