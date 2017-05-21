/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#ifndef BENCH_UTIL_H
#define BENCH_UTIL_H

/*****************************************************************/

#if defined(_WIN32)
#define BM_GIT_EXE "git.exe"
#else
#define BM_GIT_EXE "/usr/bin/git"
#endif

/*****************************************************************/


#define GITBENCH_EARGUMENTS (INT_MIN+1)

extern FILE *logfile;
extern const char *progname;
extern int verbosity;

extern int gitbench__create_tempdir(char **out);

#endif
