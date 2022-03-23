/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "openssl.h"

#ifdef GIT_SHA1_OPENSSL

int git_hash_sha1_global_init(void)
{
	return 0;
}

int git_hash_sha1_ctx_init(git_hash_sha1_ctx *ctx)
{
	return git_hash_sha1_init(ctx);
}

void git_hash_sha1_ctx_cleanup(git_hash_sha1_ctx *ctx)
{
	GIT_UNUSED(ctx);
}

int git_hash_sha1_init(git_hash_sha1_ctx *ctx)
{
	GIT_ASSERT_ARG(ctx);

	if (SHA1_Init(&ctx->c) != 1) {
		git_error_set(GIT_ERROR_SHA, "failed to initialize sha1 context");
		return -1;
	}

	return 0;
}

int git_hash_sha1_update(git_hash_sha1_ctx *ctx, const void *data, size_t len)
{
	GIT_ASSERT_ARG(ctx);

	if (SHA1_Update(&ctx->c, data, len) != 1) {
		git_error_set(GIT_ERROR_SHA, "failed to update sha1");
		return -1;
	}

	return 0;
}

int git_hash_sha1_final(unsigned char *out, git_hash_sha1_ctx *ctx)
{
	GIT_ASSERT_ARG(ctx);

	if (SHA1_Final(out, &ctx->c) != 1) {
		git_error_set(GIT_ERROR_SHA, "failed to finalize sha1");
		return -1;
	}

	return 0;
}

#endif

#ifdef GIT_SHA256_OPENSSL

int git_hash_sha256_global_init(void)
{
	return 0;
}

int git_hash_sha256_ctx_init(git_hash_sha256_ctx *ctx)
{
	return git_hash_sha256_init(ctx);
}

void git_hash_sha256_ctx_cleanup(git_hash_sha256_ctx *ctx)
{
	GIT_UNUSED(ctx);
}

int git_hash_sha256_init(git_hash_sha256_ctx *ctx)
{
	GIT_ASSERT_ARG(ctx);

	if (SHA256_Init(&ctx->c) != 1) {
		git_error_set(GIT_ERROR_SHA, "failed to initialize sha256 context");
		return -1;
	}

	return 0;
}

int git_hash_sha256_update(git_hash_sha256_ctx *ctx, const void *data, size_t len)
{
	GIT_ASSERT_ARG(ctx);

	if (SHA256_Update(&ctx->c, data, len) != 1) {
		git_error_set(GIT_ERROR_SHA, "failed to update sha256");
		return -1;
	}

	return 0;
}

int git_hash_sha256_final(unsigned char *out, git_hash_sha256_ctx *ctx)
{
	GIT_ASSERT_ARG(ctx);

	if (SHA256_Final(out, &ctx->c) != 1) {
		git_error_set(GIT_ERROR_SHA, "failed to finalize sha256");
		return -1;
	}

	return 0;
}

#endif
