/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "cli.h"
#include "buffer.h"

/*
 * This is similar to adopt's function, but modified to understand
 * that we have a command ("git") and a "subcommand" ("checkout").
 * It also understands a terminal's line length and wrap appropriately,
 * using a `git_buf` for storage.
 */
int cli_opt_usage_fprint(
	FILE *file,
	const char *command,
	const char *subcommand,
	const cli_opt_spec specs[])
{
	git_buf usage = GIT_BUF_INIT, opt = GIT_BUF_INIT;
	const cli_opt_spec *spec;
	size_t i, prefixlen, linelen;
	bool choice = false;
	int fd, console_width = 0, error;

	if ((error = git_buf_printf(&usage, "usage: %s", command)) < 0)
		goto done;

	if (subcommand &&
	    (error = git_buf_printf(&usage, " %s", subcommand)) < 0)
		goto done;

	linelen = git_buf_len(&usage);
	prefixlen = linelen + 1;

	if ((fd = fileno(file)) >= 0)
		cli_console_coords(&console_width, NULL, fd);

	for (spec = specs; spec->type; ++spec) {
		int optional = !(spec->usage & CLI_OPT_USAGE_REQUIRED);

		if (spec->usage & CLI_OPT_USAGE_HIDDEN)
			continue;

		if (choice)
			git_buf_putc(&opt, '|');
		else
			git_buf_clear(&opt);

		if (optional && !choice)
			git_buf_putc(&opt, '[');

		if (spec->type == CLI_OPT_VALUE && spec->alias)
			error = git_buf_printf(&opt, "-%c <%s>", spec->alias, spec->value_name);
		else if (spec->type == CLI_OPT_VALUE)
			error = git_buf_printf(&opt, "--%s=<%s>", spec->name, spec->value_name);
		else if (spec->type == CLI_OPT_VALUE_OPTIONAL && spec->alias)
			error = git_buf_printf(&opt, "-%c [<%s>]", spec->alias, spec->value_name);
		else if (spec->type == CLI_OPT_VALUE_OPTIONAL)
			error = git_buf_printf(&opt, "--%s[=<%s>]", spec->name, spec->value_name);
		else if (spec->type == CLI_OPT_ARG)
			error = git_buf_printf(&opt, "<%s>", spec->value_name);
		else if (spec->type == CLI_OPT_ARGS)
			error = git_buf_printf(&opt, "<%s...>", spec->value_name);
		else if (spec->type == CLI_OPT_LITERAL)
			error = git_buf_printf(&opt, "--");
		else if (spec->alias && !(spec->usage & CLI_OPT_USAGE_SHOW_LONG))
			error = git_buf_printf(&opt, "-%c", spec->alias);
		else
			error = git_buf_printf(&opt, "--%s", spec->name);

		if (error < 0)
			goto done;

		choice = !!((spec+1)->usage & CLI_OPT_USAGE_CHOICE);

		if (choice)
			continue;

		if (optional)
			git_buf_putc(&opt, ']');

		if (git_buf_oom(&opt)) {
			error = -1;
			goto done;
		}

		if (linelen > prefixlen &&
		    console_width > 0 &&
		    linelen + git_buf_len(&opt) + 1 > (size_t)console_width) {
			git_buf_putc(&usage, '\n');

			for (i = 0; i < prefixlen; i++)
				git_buf_putc(&usage, ' ');

			linelen = prefixlen;
		} else {
			git_buf_putc(&usage, ' ');
			linelen += git_buf_len(&opt) + 1;
		}

		git_buf_puts(&usage, git_buf_cstr(&opt));

		if (git_buf_oom(&usage)) {
			error = -1;
			goto done;
		}
	}

	error = fprintf(file, "%s\n", git_buf_cstr(&usage));

done:
	error = (error < 0) ? -1 : 0;

	git_buf_dispose(&usage);
	git_buf_dispose(&opt);
	return error;
}

