/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#ifndef SHELL_H
#define SHELL_H

extern int gitbench_shell(
	const char * const argv[],
	const char *new_cwd,
	int *p_raw_exit_status);

#endif
