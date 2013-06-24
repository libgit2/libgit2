#include <stdio.h>
#include <git2.h>
#include <stdlib.h>
#include <string.h>

static void check(int error, const char *message)
{
	if (error) {
		fprintf(stderr, "%s (%d)\n", message, error);
		exit(1);
	}
}

static int check_str_param(const char *arg, const char *pat, const char **val)
{
	size_t len = strlen(pat);
	if (strncmp(arg, pat, len))
		return 0;
	*val = (const char *)(arg + len);
	return 1;
}

static void usage(const char *message, const char *arg)
{
	if (message && arg)
		fprintf(stderr, "%s: %s\n", message, arg);
	else if (message)
		fprintf(stderr, "%s\n", message);
	fprintf(stderr, "usage: log [<options>]\n");
	exit(1);
}

int main(int argc, char *argv[])
{
	int i;
	char *a;
	const char *dir = ".";
	git_repository *repo;

	git_threads_init();

	for (i = 1; i < argc; ++i) {
		a = argv[i];

		if (a[0] != '-') {
		}
		else if (!check_str_param(a, "--git-dir=", &dir))
			usage("Unknown argument", a);
	}

	check(git_repository_open_ext(&repo, dir, 0, NULL),
		"Could not open repository");

	git_repository_free(repo);
	git_threads_shutdown();

	return 0;
}
