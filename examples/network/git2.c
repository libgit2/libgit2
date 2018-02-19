#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../common.h"
#include "common.h"

/* This part is not strictly libgit2-dependent, but you can use this
 * as a starting point for a git-like tool */

struct {
	char *name;
	git_cb fn;
} commands[] = {
	{"ls-remote", ls_remote},
	{"fetch", fetch},
	{"clone", do_clone},
	{"index-pack", index_pack},
	{ NULL, NULL}
};

static int run_command(git_cb fn, git_repository *repo, struct args_info args)
{
	int error;

	/* Run the command. If something goes wrong, print the error message to stderr */
	error = fn(repo, args.argc - args.pos, &args.argv[args.pos]);
	if (error < 0) {
		if (giterr_last() == NULL)
			fprintf(stderr, "Error without message");
		else
			fprintf(stderr, "Bad news:\n %s\n", giterr_last()->message);
	}

	return !!error;
}

int main(int argc, char **argv)
{
	int i;
	int return_code = 1;
	int error;
	git_repository *repo;
	struct args_info args = ARGS_INFO_INIT;
	const char *git_dir = NULL;

	if (argc < 2) {
		fprintf(stderr, "usage: %s <cmd> [repo]\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	git_libgit2_init();

	for (args.pos = 1; args.pos < args.argc; ++args.pos) {
		char *a = args.argv[args.pos];

		if (a[0] != '-') {
			/* non-arg */
			break;
		} else if (optional_str_arg(&git_dir, &args, "--git-dir", ".git")) {
			continue;
		} else if (!strcmp(a, "--")) {
			/* arg separator */
			break;
		}
	}

	/* Before running the actual command, create an instance of the local
	 * repository and pass it to the function. */

	error = git_repository_open(&repo, git_dir);
	if (error < 0)
		repo = NULL;

	for (i = 0; commands[i].name != NULL; ++i) {
		if (!strcmp(args.argv[args.pos], commands[i].name)) {
			return_code = run_command(commands[i].fn, repo, args);
			goto shutdown;
		}
	}

	fprintf(stderr, "Command not found: %s\n", argv[1]);

shutdown:
	git_repository_free(repo);

	git_libgit2_shutdown();

	return return_code;
}
