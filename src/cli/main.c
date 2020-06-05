/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include <stdio.h>
#include <git2.h>
#include <git2client.h>
#include "cli.h"
#include "cmd.h"

static int show_help = 0;
static int show_version = 0;
static char *command = NULL;
static char **args = NULL;

const cli_opt_spec cli_common_opts[] = {
	{ CLI_OPT_SWITCH, "help",      0, &show_help,    1, NULL,      "display help information" },
	{ CLI_OPT_SWITCH, "version",   0, &show_version, 1, NULL,      "display the version" },
	{ CLI_OPT_ARG,    "command",   0, &command,      0, "command", "the command to run", CLI_OPT_USAGE_REQUIRED },
	{ CLI_OPT_ARGS,   "args",      0, &args,         0, "args",    "arguments for the command" },
	{ 0 }
};

const cli_cmd_spec cli_cmds[] = {
	{ "clone", cmd_clone, "Clone a repository into a new directory" },
	{ "help",  cmd_help,  "Display help information" },
	{ NULL }
};

int main(int argc, char **argv)
{
	const cli_cmd_spec *cmd;
	cli_opt_parser optparser;
	cli_opt opt;
	char *help_args[3];
	int help_args_len;
	int args_len = 0;
	int error = 0;

	if (cli_global_init() < 0)
		cli_die("error: failed to initialize libgit2");

	cli_opt_parser_init(&optparser, cli_common_opts, argv + 1, argc - 1);

	/* Parse the top-level (common) options and command information */
	while (cli_opt_parser_next(&opt, &optparser)) {
		if (!opt.spec) {
			cli_opt_status_fprint(stderr, &opt);
			cli_opt_usage_fprint(stderr, PROGRAM_NAME, NULL, cli_common_opts);
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

	/* If "--help command" is specified, invoke "command --help" */
	if (show_help && command) {
		help_args[0] = (char *)command;
		help_args[1] = "--help";
		help_args[2] = NULL;
		help_args_len = 2;

		args = help_args;
		args_len = help_args_len;
	}

	/* If there was no command, we want to invoke "help" */
	else if (show_help || !command) {
		help_args[0] = "help";
		help_args[1] = NULL;
		help_args_len = 1;

		command = help_args[0];
		args = help_args;
		args_len = help_args_len;
	}

	if ((cmd = cli_cmd_spec_byname(command)) == NULL) {
		fprintf(stderr, "%s: '%s' is not a %s command. See '%s --help'.\n",
			PROGRAM_NAME, command, PROGRAM_NAME, PROGRAM_NAME);
		error = 1;
		goto done;
	}

	error = cmd->fn(args_len, args);

done:
	cli_global_shutdown();
	return error;
}
