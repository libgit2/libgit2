/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include <stdio.h>
#include <string.h>

#include "common.h"
#include "hash.h"

extern void hash_ppc_core(uint32_t *hash, const unsigned char *p,
			 unsigned int nblocks);

int git_hash_init(git_hash_ctx *c)
{
	c->hash[0] = 0x67452301;
	c->hash[1] = 0xEFCDAB89;
	c->hash[2] = 0x98BADCFE;
	c->hash[3] = 0x10325476;
	c->hash[4] = 0xC3D2E1F0;
	c->len = 0;
	c->cnt = 0;
	return 0;
}

int git_hash_update(git_hash_ctx *c, const void *ptr, size_t n)
{
	unsigned long nb;
	const unsigned char *p = ptr;

	c->len += (uint64_t) n << 3;
	while (n != 0) {
		if (c->cnt || n < 64) {
			nb = 64 - c->cnt;
			if (nb > n)
				nb = n;
			memcpy(&c->buf.b[c->cnt], p, nb);
			if ((c->cnt += nb) == 64) {
				hash_ppc_core(c->hash, c->buf.b, 1);
				c->cnt = 0;
			}
		} else {
			nb = n >> 6;
			hash_ppc_core(c->hash, p, nb);
			nb <<= 6;
		}
		n -= nb;
		p += nb;
	}
	return 0;
}

int git_hash_final(git_oid *oid, git_hash_ctx *c)
{
	unsigned int cnt = c->cnt;

	c->buf.b[cnt++] = 0x80;
	if (cnt > 56) {
		if (cnt < 64)
			memset(&c->buf.b[cnt], 0, 64 - cnt);
		hash_ppc_core(c->hash, c->buf.b, 1);
		cnt = 0;
	}
	if (cnt < 56)
		memset(&c->buf.b[cnt], 0, 56 - cnt);
	c->buf.l[7] = c->len;
	hash_ppc_core(c->hash, c->buf.b, 1);
	memcpy(oid->id, c->hash, 20);
	return 0;
}

