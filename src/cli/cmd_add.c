/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include <git2.h>
#include "common.h"
#include "cmd.h"

#define COMMAND_NAME "add"

static char **paths;

static const cli_opt_spec opts[] = {
	CLI_COMMON_OPT,

	{ CLI_OPT_TYPE_LITERAL },
	{ CLI_OPT_TYPE_ARGS,      "pathspecs",  0, &paths, 0,
	  CLI_OPT_USAGE_REQUIRED, "pathspecs", "the paths to add to stage" },
	{ 0 },
};

static void print_help(void)
{
	cli_opt_usage_fprint(stdout, PROGRAM_NAME, COMMAND_NAME, opts, 0);
	printf("\n");

	printf("Stage the changes in a file or files.\n");
	printf("\n");

	printf("Options:\n");

	cli_opt_help_fprint(stdout, opts);
}

int cmd_add(int argc, char **argv)
{
	cli_repository_open_options open_opts = { argv + 1, argc - 1};
	cli_opt invalid_opt;
	git_repository *repo = NULL;
	git_index *index = NULL;
	git_strarray pathspec = { 0 };
	size_t path_count = 0;
	char **path;
	int ret = 0;

	if (cli_opt_parse(&invalid_opt, opts, argv + 1, argc - 1, CLI_OPT_PARSE_GNU))
		return cli_opt_usage_error(COMMAND_NAME, opts, &invalid_opt);

	if (cli_opt__show_help) {
		print_help();
		return 0;
	}

	for (path = paths; *path; path++, path_count++)
		;

	{
	 size_t i = 0;
	 for(i = 0;i<path_count;i++) {
		printf("%d: %s\n", (int)i, paths[i]);
	 }
	}

	pathspec.strings = paths;
	pathspec.count = path_count;

	if (cli_repository_open(&repo, &open_opts) < 0 ||
	    git_repository_index(&index, repo) < 0 ||
	    git_index_add_all(index, &pathspec, 0, NULL, NULL) < 0) {
		ret = cli_error_git();
		goto done;
	}

done:
	git_index_free(index);
	git_repository_free(repo);
	return ret;
}
