/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_threadstate_h__
#define INCLUDE_threadstate_h__

#include "common.h"

typedef struct {
	git_error *last_error;
	git_error error_t;
	git_buf error_buf;
	char oid_fmt[GIT_OID_HEXSZ+1];

	/* On Windows, this is the current child thread that was started by
	 * `git_thread_create`.  This is used to set the thread's exit code
	 * when terminated by `git_thread_exit`.  It is unused on POSIX.
	 */
	git_thread *current_thread;
} git_threadstate;

extern int git_threadstate_global_init(void);
extern git_threadstate *git_threadstate_get(void);

#define GIT_THREADSTATE (git_threadstate_get())

#endif
