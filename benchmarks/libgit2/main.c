#include <stdio.h>

#include "clar.h"
#include <git2.h>

#ifdef _WIN32
int __cdecl main(int argc, char *argv[])
#else
int main(int argc, char *argv[])
#endif
{
	int res;

	clar_test_set_mode(CL_TEST_BENCHMARK);
	clar_test_init(argc, argv);

	res = git_libgit2_init();
	if (res < 0) {
		const git_error *err = git_error_last();
		const char *msg = err ? err->message : "unknown failure";
		fprintf(stderr, "failed to init libgit2: %s\n", msg);
		return res;
	}

	/* Run the test suite */
	res = clar_test_run();

	clar_test_shutdown();

	return res;
}
