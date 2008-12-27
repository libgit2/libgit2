/*
 * hash.h
 */
#ifndef INCLUDE_hash_h__
#define INCLUDE_hash_h__

#include "git/oid.h"

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
