/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_hash_h__
#define INCLUDE_hash_h__

#include "git2/oid.h"

typedef struct git_hash_ctx git_hash_ctx;

typedef struct {
	void *data;
	size_t len;
} git_buf_vec;

git_hash_ctx *git_hash_new_ctx(void);
void git_hash_free_ctx(git_hash_ctx *ctx);

void git_hash_init(git_hash_ctx *c);
void git_hash_update(git_hash_ctx *c, const void *data, size_t len);
void git_hash_final(git_oid *out, git_hash_ctx *c);

void git_hash_buf(git_oid *out, const void *data, size_t len);
void git_hash_vec(git_oid *out, git_buf_vec *vec, size_t n);

#endif /* INCLUDE_hash_h__ */
