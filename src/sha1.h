/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#ifndef INCLUDE_sha1_h__
#define INCLUDE_sha1_h__

#ifdef OPENSSL_SHA
# include <openssl/sha.h>

#else
typedef struct {
	unsigned long long size;
	unsigned int H[5];
	unsigned int W[16];
} blk_SHA_CTX;


void git__blk_SHA1_Init(blk_SHA_CTX *ctx);
void git__blk_SHA1_Update(blk_SHA_CTX *ctx, const void *dataIn, size_t len);
void git__blk_SHA1_Final(unsigned char hashout[20], blk_SHA_CTX *ctx);

#define SHA_CTX		blk_SHA_CTX
#define SHA1_Init	git__blk_SHA1_Init
#define SHA1_Update	git__blk_SHA1_Update
#define SHA1_Final	git__blk_SHA1_Final

#endif // OPENSSL_SHA

#endif
