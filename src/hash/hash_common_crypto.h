/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#ifndef INCLUDE_hash_hash_common_crypto_h__
#define INCLUDE_hash_hash_common_crypto_h__

#include "hash.h"

#include <CommonCrypto/CommonDigest.h>

struct git_hash_ctx {
	CC_SHA1_CTX c;
};

#define CC_LONG_MAX ((CC_LONG)-1)

#define git_hash_ctx_init(ctx) git_hash_init(ctx)
#define git_hash_ctx_cleanup(ctx)

GIT_INLINE(int) git_hash_global_init(void)
{
	return 0;
}

GIT_INLINE(int) git_hash_init(git_hash_ctx *ctx)
{
	assert(ctx);
	CC_SHA1_Init(&ctx->c);
	return 0;
}

GIT_INLINE(int) git_hash_update(git_hash_ctx *ctx, const void *_data, size_t len)
{
	const unsigned char *data = _data;

	assert(ctx);

	while (len > 0) {
		CC_LONG chunk = (len > CC_LONG_MAX) ? CC_LONG_MAX : (CC_LONG)len;

		CC_SHA1_Update(&ctx->c, data, chunk);

		data += chunk;
		len -= chunk;
	}

	return 0;
}

GIT_INLINE(int) git_hash_final(git_oid *out, git_hash_ctx *ctx)
{
	assert(ctx);
	CC_SHA1_Final(out->id, &ctx->c);
	return 0;
}

#endif
