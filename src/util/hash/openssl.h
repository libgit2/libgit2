/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#ifndef INCLUDE_hash_openssl_h__
#define INCLUDE_hash_openssl_h__

#include "hash/sha.h"

#include <openssl/sha.h>

#ifdef GIT_SHA1_OPENSSL
struct git_hash_sha1_ctx {
	SHA_CTX c;
};
#endif

#ifdef GIT_SHA256_OPENSSL
struct git_hash_sha256_ctx {
	SHA256_CTX c;
};
#endif

#endif
