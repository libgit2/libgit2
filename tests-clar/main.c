#include "clar_libgit2.h"

#ifdef _WIN32
__cdecl int
#else
int
#endif
main(int argc, char *argv[])
{
	int res;

	git_threads_init();

	/* Run the test suite */
	res = clar_test(argc, argv);

	giterr_clear();
	git_threads_shutdown();

	return res;
}
