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

#define COMMAND_NAME "version"

static int build_options;

struct feature_names {
	int feature;
	const char *name;
};

static const struct feature_names feature_names[] = {
	{ GIT_FEATURE_SHA1,           "sha1"           },
	{ GIT_FEATURE_SHA256,         "sha256"         },
	{ GIT_FEATURE_THREADS,        "threads"        },
	{ GIT_FEATURE_NSEC,           "nsec"           },
	{ GIT_FEATURE_COMPRESSION,    "compression"    },
	{ GIT_FEATURE_I18N,           "i18n"           },
	{ GIT_FEATURE_REGEX,          "regex"          },
	{ GIT_FEATURE_SSH,            "ssh"            },
	{ GIT_FEATURE_HTTPS,          "https"          },
	{ GIT_FEATURE_HTTP_PARSER,    "http_parser"    },
	{ GIT_FEATURE_AUTH_NTLM,      "auth_ntlm"      },
	{ GIT_FEATURE_AUTH_NEGOTIATE, "auth_negotiate" },
	{ 0, NULL }
};

static const cli_opt_spec opts[] = {
	CLI_COMMON_OPT,

	{ CLI_OPT_TYPE_SWITCH,    "build-options",  0,  &build_options, 1,
	  CLI_OPT_USAGE_DEFAULT,   NULL,           "show compile-time options" },
	{ 0 },
};

static int print_help(void)
{
	cli_opt_usage_fprint(stdout, PROGRAM_NAME, COMMAND_NAME, opts, 0);
	printf("\n");

	printf("Display version information for %s.\n", PROGRAM_NAME);
	printf("\n");

	printf("Options:\n");

	cli_opt_help_fprint(stdout, opts);

	return 0;
}

int cmd_version(int argc, char **argv)
{
	cli_opt invalid_opt;
	const struct feature_names *f;
	const char *backend;
	int supported_features;

	if (cli_opt_parse(&invalid_opt, opts, argv + 1, argc - 1, CLI_OPT_PARSE_GNU))
		return cli_opt_usage_error(COMMAND_NAME, opts, &invalid_opt);

	if (cli_opt__show_help) {
		print_help();
		return 0;
	}

	printf("%s version %s\n", PROGRAM_NAME, LIBGIT2_VERSION);

	if (build_options) {
		supported_features = git_libgit2_features();

		for (f = feature_names; f->feature; f++) {
			if (!(supported_features & f->feature))
				continue;

			backend = git_libgit2_feature_backend(f->feature);
			printf("backend-%s: %s\n", f->name, backend);
		}
	}

	return 0;
}
