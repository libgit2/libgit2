/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include <stdio.h>
#include <git2.h>
#include "cli.h"

static int show_version = 0;
static char *command = NULL;
static char **args = NULL;

static const cli_opt_spec common_opts[] = {
	{ CLI_OPT_SWITCH, "version",   0, &show_version, 1, NULL,      "display the version" },
	{ CLI_OPT_ARG,    "command",   0, &command,      0, "command", "the command to run", CLI_OPT_USAGE_REQUIRED },
	{ CLI_OPT_ARGS,   "args",      0, &args,         0, "args",    "arguments for the command" },
	{ 0 }
};

int main(int argc, char **argv)
{
	cli_opt_parser optparser;
	cli_opt opt;

	int args_len = 0;
	int error = 0;

	if (git_libgit2_init() < 0) {
		fprintf(stderr, "error: failed to initialize libgit2\n");
		exit(1);
	}

	cli_opt_parser_init(&optparser, common_opts, argv + 1, argc - 1);

	/* Parse the top-level (common) options and command information */
	while (cli_opt_parser_next(&opt, &optparser)) {
		if (!opt.spec) {
			cli_opt_status_fprint(stderr, &opt);
			error = 129;
			goto done;
		}

		/*
		 * When we see a command, stop parsing and capture the
		 * remaining arguments as args for the command itself.
		 */
		if (command) {
			args = &argv[optparser.idx];
			args_len = (int)(argc - optparser.idx);
			break;
		}
	}

	if (show_version) {
		printf("%s version %s\n", PROGRAM_NAME, LIBGIT2_VERSION);
		goto done;
	}

done:
	git_libgit2_shutdown();
	return error;
}
