/*
 * Copyright (C) 2009-2012 the libgit2 contributors
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

GIT_INLINE(git_hash_ctx *) git_hash_ctx_new(void)
{
	git_hash_ctx *ctx = git__malloc(sizeof(git_hash_ctx));

	if (!ctx)
		return NULL;

	SHA1_Init(&ctx->c);

	return ctx;
}

GIT_INLINE(void) git_hash_ctx_free(git_hash_ctx *ctx)
{
	if (ctx)
		git__free(ctx);
}

GIT_INLINE(int) git_hash_init(git_hash_ctx *ctx)
{
	assert(ctx);
	SHA1_Init(&ctx->c);
	return 0;
}

GIT_INLINE(int) git_hash_update(git_hash_ctx *ctx, const void *data, size_t len)
{
	assert(ctx);
	SHA1_Update(&ctx->c, data, len);
	return 0;
}

GIT_INLINE(int) git_hash_final(git_oid *out, git_hash_ctx *ctx)
{
	assert(ctx);
	SHA1_Final(out->id, &ctx->c);
	return 0;
}

#endif /* INCLUDE_hash_openssl_h__ */
