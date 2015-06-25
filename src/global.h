/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_global_h__
#define INCLUDE_global_h__

#include "common.h"
#include "mwindow.h"
#include "hash.h"

typedef struct {
	/* last error that occurred, may be the static OOM message */
	const char *last_error;

	/* last error message allocated, should be freed */
	char *error_buf;

	char oid_fmt[GIT_OID_HEXSZ+1];
} git_global_st;

#ifdef GIT_OPENSSL
# include <openssl/ssl.h>
extern SSL_CTX *git__ssl_ctx;
#endif

git_global_st *git__global_state(void);

extern git_mutex git__mwindow_mutex;

#define GIT_GLOBAL (git__global_state())

typedef void (*git_global_shutdown_fn)(void);

extern void git__on_shutdown(git_global_shutdown_fn callback);

extern void git__free_tls_data(void);

#endif
