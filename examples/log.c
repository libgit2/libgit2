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
	int i, j, last_nonoption, force_files = -1;
	char *a;
	const char *dir = ".";
	git_repository *repo;
	git_revwalk *walker;
	git_revspec revs;

	git_threads_init();

	for (i = 1, last_nonoption = 1; i < argc; ++i) {
		a = argv[i];

		if (a[0] != '-' || force_files > 0) {
			/* condense args not prefixed with '-' to start of argv */
			if (last_nonoption != i)
				argv[last_nonoption] = a;
			last_nonoption++;
		}
		else if (!strcmp(a, "--"))
			force_files = last_nonoption; /* copy all args as filenames */
		else if (!check_str_param(a, "--git-dir=", &dir))
			usage("Unknown argument", a);
	}

	check(git_repository_open_ext(&repo, dir, 0, NULL),
		"Could not open repository");
	check(git_revwalk_new(&walker, repo),
		"Could not create revision walker");

	if (force_files < 0)
		force_files = last_nonoption;

	for (i = 1; i < force_files; ) {
		printf("option '%s'\n", argv[i]);

		if (!git_revparse(&revs, repo, argv[i])) {
			char str[GIT_OID_HEXSZ+1];

			if (revs.from) {
				git_oid_tostr(str, sizeof(str), git_object_id(revs.from));
				printf("revwalk from %s\n", str);
			}
			if (revs.to) {
				git_oid_tostr(str, sizeof(str), git_object_id(revs.to));
				printf("revwalk to %s\n", str);
			}

			/* push / hide / merge-base in revwalker */

			++i;
		} else {
			/* shift array down */
			for (a = argv[i], j = i + 1; j < force_files; ++j)
				argv[j - 1] = argv[j];
			argv[--force_files] = a;
		}
	}

	if (i == 1) {
		/* no revs pushed so push HEAD */
		printf("revwalk HEAD\n");
	}

	for (i = force_files; i < last_nonoption; ++i)
		printf("file %s\n", argv[i]);

	git_repository_free(repo);
	git_threads_shutdown();

	return 0;
}
