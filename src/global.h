/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_global_h__
#define INCLUDE_global_h__

#include "mwindow.h"

typedef struct {
	struct {
		char last[1024];
	} error;

	git_error *last_error;
	git_error error_t;

	git_mwindow_ctl mem_ctl;
} git_global_st;

git_global_st *git__global_state(void);

#define GIT_GLOBAL (git__global_state())

#endif
