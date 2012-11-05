/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_global_h__
#define INCLUDE_global_h__

#include "mwindow.h"
#include "hash.h"

typedef struct {
	git_error *last_error;
	git_error error_t;

#ifdef WIN32_SHA1
	git_hash_prov hash_prov;
#endif

} git_global_st;

git_global_st *git__global_state(void);

extern git_mutex git__mwindow_mutex;

#define GIT_GLOBAL (git__global_state())

#endif
