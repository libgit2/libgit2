/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#ifndef INCLUDE_hash_ppc_h__
#define INCLUDE_hash_ppc_h__

#include <stdint.h>

struct git_hash_ctx {
	uint32_t hash[5];
	uint32_t cnt;
	uint64_t len;
	union {
		unsigned char b[64];
		uint64_t l[8];
	} buf;
};

#define git_hash_global_init() 0
#define git_hash_global_shutdown() /* noop */
#define git_hash_ctx_init(ctx) git_hash_init(ctx)
#define git_hash_ctx_cleanup(ctx)

#endif /* INCLUDE_hash_generic_h__ */
