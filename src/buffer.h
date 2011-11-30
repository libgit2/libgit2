/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_buffer_h__
#define INCLUDE_buffer_h__

#include "common.h"

typedef struct {
	char *ptr;
	ssize_t asize, size;
} git_buf;

#define GIT_BUF_INIT {NULL, 0, 0}

int git_buf_grow(git_buf *buf, size_t target_size);
void git_buf_free(git_buf *buf);
void git_buf_swap(git_buf *buf_a, git_buf *buf_b);

/**
 * Any function that writes to a git_buf can fail due to memory allocation
 * issues.  If one fails, the git_buf will be marked with an OOM error and
 * further calls to modify the buffer will fail.  You just check
 * git_buf_oom() at the end of your sequence and it will be true if you ran
 * out of memory at any point with that buffer.
 */
int git_buf_oom(const git_buf *buf);

void git_buf_set(git_buf *buf, const char *data, size_t len);
void git_buf_sets(git_buf *buf, const char *string);
void git_buf_putc(git_buf *buf, char c);
void git_buf_put(git_buf *buf, const char *data, size_t len);
void git_buf_puts(git_buf *buf, const char *string);
void git_buf_printf(git_buf *buf, const char *format, ...) GIT_FORMAT_PRINTF(2, 3);
void git_buf_clear(git_buf *buf);
void git_buf_consume(git_buf *buf, const char *end);
void git_buf_join_n(git_buf *buf, char separator, int nbuf, ...);
void git_buf_join(git_buf *buf, char separator, const char *str_a, const char *str_b);

const char *git_buf_cstr(git_buf *buf);
char *git_buf_take_cstr(git_buf *buf);
void git_buf_copy_cstr(char *data, size_t datasize, git_buf *buf);

#define git_buf_PUTS(buf, str) git_buf_put(buf, str, sizeof(str) - 1)

#endif
