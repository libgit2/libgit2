/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#ifndef INCLUDE_hash_openssl_h__
#define INCLUDE_hash_openssl_h__

#include "hash.h"

#include <openssl/sha.h>

struct git_hash_ctx {
	SHA_CTX c;
};

#define git_hash_global_init() 0
#define git_hash_ctx_init(ctx) git_hash_init(ctx)
#define git_hash_ctx_cleanup(ctx)

GIT_INLINE(int) git_hash_init(git_hash_ctx *ctx)
{
	assert(ctx);

	if (SHA1_Init(&ctx->c) != 1) {
		giterr_set(GITERR_SHA1, "hash_openssl: failed to initialize hash context");
		return -1;
	}

	return 0;
}

GIT_INLINE(int) git_hash_update(git_hash_ctx *ctx, const void *data, size_t len)
{
	assert(ctx);

	if (SHA1_Update(&ctx->c, data, len) != 1) {
		giterr_set(GITERR_SHA1, "hash_openssl: failed to update hash");
		return -1;
	}

	return 0;
}

GIT_INLINE(int) git_hash_final(git_oid *out, git_hash_ctx *ctx)
{
	assert(ctx);

	if (SHA1_Final(out->id, &ctx->c) != 1) {
		giterr_set(GITERR_SHA1, "hash_openssl: failed to finalize hash");
		return -1;
	}

	return 0;
}

#endif /* INCLUDE_hash_openssl_h__ */
