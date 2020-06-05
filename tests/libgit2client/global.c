#include "clar_libgit2.h"
#include "git2client_tests.h"

int git_global_test_init(void)
{
	int res = git_client_init();

	if (res < 0)
		fprintf(stderr, "failed to init allocator");

	return res;
}

void git_global_test_shutdown(void)
{
	git_client_shutdown();
}
