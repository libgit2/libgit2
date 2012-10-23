#include "clar_libgit2.h"

static const char *_name = "remote.fancy.url";

void test_config_multivar__initialize(void)
{
	cl_fixture_sandbox("config");
}

void test_config_multivar__cleanup(void)
{
	cl_fixture_cleanup("config");
}

static int mv_read_cb(const git_config_entry *entry, void *data)
{
	int *n = (int *) data;

	if (!strcmp(entry->name, _name))
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

static int cb(const git_config_entry *entry, void *data)
{
	int *n = (int *) data;

	GIT_UNUSED(entry);

	(*n)++;

	return 0;
}

void test_config_multivar__get(void)
{
	git_config *cfg;
	int n;

	cl_git_pass(git_config_open_ondisk(&cfg, "config/config11"));

	n = 0;
	cl_git_pass(git_config_get_multivar(cfg, _name, NULL, cb, &n));
	cl_assert(n == 2);

	n = 0;
	cl_git_pass(git_config_get_multivar(cfg, _name, "example", cb, &n));
	cl_assert(n == 1);

	git_config_free(cfg);
}

void test_config_multivar__add(void)
{
	git_config *cfg;
	int n;

	cl_git_pass(git_config_open_ondisk(&cfg, "config/config11"));
	cl_git_pass(git_config_set_multivar(cfg, _name, "nonexistant", "git://git.otherplace.org/libgit2"));

	n = 0;
	cl_git_pass(git_config_get_multivar(cfg, _name, NULL, cb, &n));
	cl_assert(n == 3);

	n = 0;
	cl_git_pass(git_config_get_multivar(cfg, _name, "otherplace", cb, &n));
	cl_assert(n == 1);

	git_config_free(cfg);

	/* We know it works in memory, let's see if the file is written correctly */

	cl_git_pass(git_config_open_ondisk(&cfg, "config/config11"));

	n = 0;
	cl_git_pass(git_config_get_multivar(cfg, _name, NULL, cb, &n));
	cl_assert(n == 3);

	n = 0;
	cl_git_pass(git_config_get_multivar(cfg, _name, "otherplace", cb, &n));
	cl_assert(n == 1);

	git_config_free(cfg);
}

void test_config_multivar__replace(void)
{
	git_config *cfg;
	int n;

	cl_git_pass(git_config_open_ondisk(&cfg, "config/config11"));

	n = 0;
	cl_git_pass(git_config_get_multivar(cfg, _name, NULL, cb, &n));
	cl_assert(n == 2);

	cl_git_pass(git_config_set_multivar(cfg, _name, "github", "git://git.otherplace.org/libgit2"));

	n = 0;
	cl_git_pass(git_config_get_multivar(cfg, _name, NULL, cb, &n));
	cl_assert(n == 2);

	git_config_free(cfg);

	cl_git_pass(git_config_open_ondisk(&cfg, "config/config11"));

	n = 0;
	cl_git_pass(git_config_get_multivar(cfg, _name, NULL, cb, &n));
	cl_assert(n == 2);

	git_config_free(cfg);
}

void test_config_multivar__replace_multiple(void)
{
	git_config *cfg;
	int n;

	cl_git_pass(git_config_open_ondisk(&cfg, "config/config11"));
	cl_git_pass(git_config_set_multivar(cfg, _name, "git://", "git://git.otherplace.org/libgit2"));

	n = 0;
	cl_git_pass(git_config_get_multivar(cfg, _name, "otherplace", cb, &n));
	cl_assert(n == 2);

	git_config_free(cfg);

	cl_git_pass(git_config_open_ondisk(&cfg, "config/config11"));

	n = 0;
	cl_git_pass(git_config_get_multivar(cfg, _name, "otherplace", cb, &n));
	cl_assert(n == 2);

	git_config_free(cfg);
}
