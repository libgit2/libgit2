/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_tlsdata_h__
#define INCLUDE_tlsdata_h__

#include "common.h"

typedef struct {
	git_error *last_error;
	git_error error_t;
	git_buf error_buf;
	char oid_fmt[GIT_OID_HEXSZ+1];
} git_tlsdata;

extern int git_tlsdata_global_init(void);
extern git_tlsdata *git_tlsdata_get(void);

#define GIT_TLSDATA (git_tlsdata_get())

#endif
