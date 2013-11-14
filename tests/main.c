#include "clar_libgit2.h"

#ifdef _WIN32
int __cdecl main(int argc, char *argv[])
#else
int main(int argc, char *argv[])
#endif
{
	int res;

	git_threads_init();

	/* Run the test suite */
	res = clar_test(argc, argv);

	giterr_clear();
	git_threads_shutdown();

	return res;
}
