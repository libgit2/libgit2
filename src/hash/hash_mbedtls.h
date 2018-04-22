/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#ifndef INCLUDE_hash_mbedtld_h__
#define INCLUDE_hash_mbedtld_h__

#include <mbedtls/sha1.h>

struct git_hash_ctx {
    mbedtls_sha1_context c;
};

#define git_hash_global_init() 0
#define git_hash_ctx_init(ctx) git_hash_init(ctx)

#endif /* INCLUDE_hash_mbedtld_h__ */
