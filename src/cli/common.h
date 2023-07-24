/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#ifndef CLI_common_h__
#define CLI_common_h__

#define PROGRAM_NAME "git2"

#include "git2_util.h"

#include "error.h"
#include "opt.h"
#include "opt_usage.h"

/*
 * Common command arguments.
 */

#define CLI_COMMON_OPT_HELP \
	CLI_OPT_TYPE_SWITCH, "help",       0, &show_help, 1, \
	CLI_OPT_USAGE_HIDDEN | CLI_OPT_USAGE_STOP_PARSING
#define CLI_COMMON_OPT_CONFIG \
	CLI_OPT_TYPE_VALUE,   NULL,       'c', NULL,      0, \
	CLI_OPT_USAGE_HIDDEN
#define CLI_COMMON_OPT_CONFIG_ENV \
	CLI_OPT_TYPE_VALUE,  "config-env", 0,  NULL,      0, \
	CLI_OPT_USAGE_HIDDEN

#define CLI_COMMON_OPT \
	{ CLI_COMMON_OPT_HELP }, \
	{ CLI_COMMON_OPT_CONFIG }, \
	{ CLI_COMMON_OPT_CONFIG_ENV }

typedef struct {
	char **args;
	int args_len;
} cli_repository_open_options;

extern int cli_repository_open(
	git_repository **out,
	cli_repository_open_options *opts);

/*
 * Common command arguments.
 */

#define CLI_COMMON_OPT_HELP \
	CLI_OPT_TYPE_SWITCH, "help",       0, &show_help, 1, \
	CLI_OPT_USAGE_HIDDEN | CLI_OPT_USAGE_STOP_PARSING
#define CLI_COMMON_OPT_CONFIG \
	CLI_OPT_TYPE_VALUE,   NULL,       'c', NULL,      0, \
	CLI_OPT_USAGE_HIDDEN
#define CLI_COMMON_OPT_CONFIG_ENV \
	CLI_OPT_TYPE_VALUE,  "config-env", 0,  NULL,      0, \
	CLI_OPT_USAGE_HIDDEN

#define CLI_COMMON_OPT \
	{ CLI_COMMON_OPT_HELP }, \
	{ CLI_COMMON_OPT_CONFIG }, \
	{ CLI_COMMON_OPT_CONFIG_ENV }

#endif /* CLI_common_h__ */
