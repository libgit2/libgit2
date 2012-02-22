#include "clar_libgit2.h"

static int mv_read_cb(const char *name, const char *GIT_UNUSED(value), void *data)
{
	int *n = (int *) data;

	if (!strcmp(name, "remote.fancy.url"))
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

static int cb(const char *GIT_UNUSED(val), void *data)
{
	int *n = (int *) data;

	(*n)++;

	return GIT_SUCCESS;
}

void test_config_multivar__get(void)
{
	git_config *cfg;
	const char *name = "remote.fancy.url";
	int n;

	cl_git_pass(git_config_open_ondisk(&cfg, cl_fixture("config/config11")));

	n = 0;
	cl_git_pass(git_config_get_multivar(cfg, name, NULL, cb, &n));
	cl_assert(n == 2);

	n = 0;
	cl_git_pass(git_config_get_multivar(cfg, name, "example", cb, &n));
	cl_assert(n == 1);

	git_config_free(cfg);
}

void test_config_multivar__add(void)
{
	git_config *cfg;
	const char *name = "remote.fancy.url";
	int n;

	cl_fixture_sandbox("config");
	cl_git_pass(git_config_open_ondisk(&cfg, "config/config11"));
	cl_git_pass(git_config_set_multivar(cfg, name, "^$", "git://git.otherplace.org/libgit2"));

	n = 0;
	cl_git_pass(git_config_get_multivar(cfg, name, NULL, cb, &n));
	cl_assert(n == 3);

	n = 0;
	cl_git_pass(git_config_get_multivar(cfg, name, "otherplace", cb, &n));
	cl_assert(n == 1);

	git_config_free(cfg);

	/* We know it works in memory, let's see if the file is written correctly */

	cl_git_pass(git_config_open_ondisk(&cfg, "config/config11"));

	n = 0;
	cl_git_pass(git_config_get_multivar(cfg, name, NULL, cb, &n));
	cl_assert(n == 3);


	n = 0;
	cl_git_pass(git_config_get_multivar(cfg, name, "otherplace", cb, &n));
	cl_assert(n == 1);

	git_config_free(cfg);
}
