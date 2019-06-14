/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "hash_collisiondetect.h"

int git_hash_global_init(void)
{
	return 0;
}

int git_hash_ctx_init(git_hash_ctx *ctx)
{
	return git_hash_init(ctx);
}

void git_hash_ctx_cleanup(git_hash_ctx *ctx)
{
	GIT_UNUSED(ctx);
}

int git_hash_init(git_hash_ctx *ctx)
{
	assert(ctx);
	SHA1DCInit(&ctx->c);
	return 0;
}

int git_hash_update(git_hash_ctx *ctx, const void *data, size_t len)
{
	assert(ctx);
	SHA1DCUpdate(&ctx->c, data, len);
	return 0;
}

int git_hash_final(git_oid *out, git_hash_ctx *ctx)
{
	assert(ctx);
	if (SHA1DCFinal(out->id, &ctx->c)) {
		git_error_set(GIT_ERROR_SHA1, "SHA1 collision attack detected");
		return -1;
	}

	return 0;
}
