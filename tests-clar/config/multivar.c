#include "clar_libgit2.h"

static int mv_read_cb(const char *name, const char *GIT_UNUSED(value), void *data)
{
	int *n = (int *) data;

	if (!strcmp(name, "remote.fancy.fetch"))
		(*n)++;

	return 0;
}

void test_config_multivar__foreach(void)
{
	git_config *cfg;
	int n = 0;

	cl_git_pass(git_config_open_ondisk(&cfg, cl_fixture("config/config11")));

	cl_git_pass(git_config_foreach(cfg, mv_read_cb, &n));
	cl_assert(n == 2);

	git_config_free(cfg);
}
