/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#ifndef CLI_console_h__
#define CLI_console_h__

/**
 * Prompts for a password, placing the results into a `git_str`.
 *
 * This data should be cleared from memory by overwriting it, using
 * `git_str_zero`.
 *
 * @param out the password
 * @param prompt the prompt to write to the tty
 * @return 0 on success, -1 on failure
 */

int cli_console_getpass(git_str *out, const char *prompt);

#endif /* CLI_console_h__ */
