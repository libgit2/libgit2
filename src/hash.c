/*
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2,
 * as published by the Free Software Foundation.
 *
 * In addition to the permissions in the GNU General Public License,
 * the authors give you unlimited permission to link the compiled
 * version of this file into combinations with other programs,
 * and to distribute those combinations without any restriction
 * coming from the use of this file.  (The General Public License
 * restrictions do apply in other respects; for example, they cover
 * modification of the file, and distribution when not linked into
 * a combined executable.)
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "common.h"
#include "hash.h"
#include "sha1.h"

struct git_hash_ctx {
	SHA_CTX c;
};

git_hash_ctx *git_hash_new_ctx(void)
{
	git_hash_ctx *ctx = git__malloc(sizeof(*ctx));

	if (!ctx)
		return NULL;

	SHA1_Init(&ctx->c);

	return ctx;
}

void git_hash_free_ctx(git_hash_ctx *ctx)
{
	free(ctx);
}

void git_hash_init(git_hash_ctx *ctx)
{
	assert(ctx);
	SHA1_Init(&ctx->c);
}

void git_hash_update(git_hash_ctx *ctx, const void *data, size_t len)
{
	assert(ctx);
	SHA1_Update(&ctx->c, data, len);
}

void git_hash_final(git_oid *out, git_hash_ctx *ctx)
{
	assert(ctx);
	SHA1_Final(out->id, &ctx->c);
}

void git_hash_buf(git_oid *out, const void *data, size_t len)
{
	SHA_CTX c;

	SHA1_Init(&c);
	SHA1_Update(&c, data, len);
	SHA1_Final(out->id, &c);
}

void git_hash_vec(git_oid *out, git_buf_vec *vec, size_t n)
{
	SHA_CTX c;
	size_t i;

	SHA1_Init(&c);
	for (i = 0; i < n; i++)
		SHA1_Update(&c, vec[i].data, vec[i].len);
	SHA1_Final(out->id, &c);
}
