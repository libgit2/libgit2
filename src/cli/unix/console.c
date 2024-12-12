/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include <stdint.h>
#include <signal.h>
#include "git2_util.h"
#include "common.h"
#include "console.h"

#include <readpassphrase.h>

int cli_console_getpass(git_str *out, const char *prompt)
{
	char buf[1024];

	if (readpassphrase(prompt, buf, sizeof(buf), 0) == NULL) {
		git_error_set(GIT_ERROR_OS, "could not read passphrase from tty");
		return -1;
	}

	if (git_str_puts(out, buf) < 0)
		return -1;

	git__memzero(buf, sizeof(buf));
	return 0;
}
