#include "clar_libgit2.h"

int git_global_test_init(void)
{
	int res = git_libgit2_init();

	if (res < 0)
		fprintf(stderr, "failed to init libgit2");

	return res;
}

void git_global_test_shutdown(void)
{
	git_libgit2_shutdown();
}
