/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include <stdio.h>
#include <git2.h>
#include "common.h"
#include "cmd.h"
#include "error.h"
#include "sighandler.h"
#include "progress.h"

#include "fs_path.h"
#include "futils.h"

#define COMMAND_NAME "blame"

static char *file;
static int show_help;

static const cli_opt_spec opts[] = {
	CLI_COMMON_OPT,

	{ CLI_OPT_TYPE_LITERAL },
	{ CLI_OPT_TYPE_ARG,       "file",         0,  &file, 0,
	  CLI_OPT_USAGE_REQUIRED, "file",        "file to blame" },

	{ 0 }
};

static void print_help(void)
{
	cli_opt_usage_fprint(stdout, PROGRAM_NAME, COMMAND_NAME, opts);
	printf("\n");

	printf("Show the origin of each line of a file.\n");
	printf("\n");

	printf("Options:\n");

	cli_opt_help_fprint(stdout, opts);
}

int cmd_blame(int argc, char **argv)
{
	cli_repository_open_options open_opts = { argv + 1, argc - 1 };
	git_blame_options blame_opts = GIT_BLAME_OPTIONS_INIT;
	git_repository *repo = NULL;
	git_blame *blame = NULL;
	cli_opt invalid_opt;
	int ret = 0;

	if (cli_opt_parse(&invalid_opt, opts, argv + 1, argc - 1, CLI_OPT_PARSE_GNU))
		return cli_opt_usage_error(COMMAND_NAME, opts, &invalid_opt);

	if (show_help) {
		print_help();
		return 0;
	}

	if (!file) {
		ret = cli_error_usage("you must specify a file to blame");
		goto done;
	}

	if (cli_repository_open(&repo, &open_opts) < 0)
		return cli_error_git();

	if (git_blame_file(&blame, repo, file, &blame_opts) < 0) {
		ret = cli_error_git();
		goto done;
	}

done:
	git_blame_free(blame);
	git_repository_free(repo);
	return ret;
}
