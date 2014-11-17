/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#ifndef INCLUDE_hash_gnutls_h__
#define INCLUDE_hash_gnutls_h__

#include "hash.h"

#include <gnutls/gnutls.h>
#include <gnutls/crypto.h>

struct git_hash_ctx {
	gnutls_hash_hd_t c;
};

#define git_hash_global_init() 0
#define git_hash_ctx_init(ctx) git_hash_init(ctx)
#define git_hash_ctx_cleanup(ctx) git_hash_cleanup(ctx)

GIT_INLINE(int) git_hash_init(git_hash_ctx *ctx)
{
	int error;
	assert(ctx);
	if ((error = gnutls_hash_init(&ctx->c, GNUTLS_MAC_SHA1)) < 0) {
		giterr_set(GITERR_SSL, "gnutls: %s", gnutls_strerror(error));
		return -1;
	}
	return 0;
}

GIT_INLINE(int) git_hash_update(git_hash_ctx *ctx, const void *data, size_t len)
{
	int error;
	assert(ctx);
	if ((error = gnutls_hash(ctx->c, data, len)) < 0) {
		giterr_set(GITERR_SSL, "gnutls: %s", gnutls_strerror(error));
		return -1;
	}
	return 0;
}

GIT_INLINE(int) git_hash_final(git_oid *out, git_hash_ctx *ctx)
{
	assert(ctx);
	gnutls_hash_output(ctx->c, out);
	return 0;
}

GIT_INLINE(void) git_hash_cleanup(git_hash_ctx *ctx)
{
	gnutls_hash_deinit(ctx->c, NULL);
}

#endif
