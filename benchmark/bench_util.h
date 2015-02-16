/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#ifndef BENCH_UTIL_H
#define BENCH_UTIL_H

#define GITBENCH_EARGUMENTS (INT_MIN+1)

extern const char *progname;
extern int verbosity;

extern int gitbench__create_tempdir(char **out);

#endif
