/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_buffer_h__
#define INCLUDE_buffer_h__

#include "common.h"
#include "git2/strarray.h"
#include <stdarg.h>

typedef struct {
	char *ptr;
	size_t asize, size;
} git_buf;

extern char git_buf__initbuf[];
extern char git_buf__oom[];

#define GIT_BUF_INIT { git_buf__initbuf, 0, 0 }

/**
 * Initialize a git_buf structure.
 *
 * For the cases where GIT_BUF_INIT cannot be used to do static
 * initialization.
 */
extern void git_buf_init(git_buf *buf, size_t initial_size);

/**
 * Attempt to grow the buffer to hold at least `target_size` bytes.
 *
 * If the allocation fails, this will return an error.  If mark_oom is true,
 * this will mark the buffer as invalid for future operations; if false,
 * existing buffer content will be preserved, but calling code must handle
 * that buffer was not expanded.
 */
extern int git_buf_try_grow(git_buf *buf, size_t target_size, bool mark_oom);

/**
 * Grow the buffer to hold at least `target_size` bytes.
 *
 * If the allocation fails, this will return an error and the buffer will be
 * marked as invalid for future operations, invaliding contents.
 *
 * @return 0 on success or -1 on failure
 */
GIT_INLINE(int) git_buf_grow(git_buf *buf, size_t target_size)
{
	return git_buf_try_grow(buf, target_size, true);
}

extern void git_buf_free(git_buf *buf);
extern void git_buf_swap(git_buf *buf_a, git_buf *buf_b);
extern char *git_buf_detach(git_buf *buf);
extern void git_buf_attach(git_buf *buf, char *ptr, size_t asize);

/**
 * Test if there have been any reallocation failures with this git_buf.
 *
 * Any function that writes to a git_buf can fail due to memory allocation
 * issues.  If one fails, the git_buf will be marked with an OOM error and
 * further calls to modify the buffer will fail.  Check git_buf_oom() at the
 * end of your sequence and it will be true if you ran out of memory at any
 * point with that buffer.
 *
 * @return false if no error, true if allocation error
 */
GIT_INLINE(bool) git_buf_oom(const git_buf *buf)
{
	return (buf->ptr == git_buf__oom);
}

/*
 * Functions below that return int value error codes will return 0 on
 * success or -1 on failure (which generally means an allocation failed).
 * Using a git_buf where the allocation has failed with result in -1 from
 * all further calls using that buffer.  As a result, you can ignore the
 * return code of these functions and call them in a series then just call
 * git_buf_oom at the end.
 */
int git_buf_set(git_buf *buf, const char *data, size_t len);
int git_buf_sets(git_buf *buf, const char *string);
int git_buf_putc(git_buf *buf, char c);
int git_buf_put(git_buf *buf, const char *data, size_t len);
int git_buf_puts(git_buf *buf, const char *string);
int git_buf_printf(git_buf *buf, const char *format, ...) GIT_FORMAT_PRINTF(2, 3);
int git_buf_vprintf(git_buf *buf, const char *format, va_list ap);
void git_buf_clear(git_buf *buf);
void git_buf_consume(git_buf *buf, const char *end);
void git_buf_truncate(git_buf *buf, size_t len);
void git_buf_rtruncate_at_char(git_buf *path, char separator);

int git_buf_join_n(git_buf *buf, char separator, int nbuf, ...);
int git_buf_join(git_buf *buf, char separator, const char *str_a, const char *str_b);

/**
 * Join two strings as paths, inserting a slash between as needed.
 * @return 0 on success, -1 on failure
 */
GIT_INLINE(int) git_buf_joinpath(git_buf *buf, const char *a, const char *b)
{
	return git_buf_join(buf, '/', a, b);
}

GIT_INLINE(const char *) git_buf_cstr(const git_buf *buf)
{
	return buf->ptr;
}

GIT_INLINE(size_t) git_buf_len(const git_buf *buf)
{
	return buf->size;
}

void git_buf_copy_cstr(char *data, size_t datasize, const git_buf *buf);

#define git_buf_PUTS(buf, str) git_buf_put(buf, str, sizeof(str) - 1)

GIT_INLINE(ssize_t) git_buf_rfind_next(git_buf *buf, char ch)
{
	ssize_t idx = (ssize_t)buf->size - 1;
	while (idx >= 0 && buf->ptr[idx] == ch) idx--;
	while (idx >= 0 && buf->ptr[idx] != ch) idx--;
	return idx;
}

GIT_INLINE(ssize_t) git_buf_rfind(git_buf *buf, char ch)
{
	ssize_t idx = (ssize_t)buf->size - 1;
	while (idx >= 0 && buf->ptr[idx] != ch) idx--;
	return idx;
}

GIT_INLINE(ssize_t) git_buf_find(git_buf *buf, char ch)
{
	size_t idx = 0;
	while (idx < buf->size && buf->ptr[idx] != ch) idx++;
	return (idx == buf->size) ? -1 : (ssize_t)idx;
}

/* Remove whitespace from the end of the buffer */
void git_buf_rtrim(git_buf *buf);

int git_buf_cmp(const git_buf *a, const git_buf *b);

/* Write data as base64 encoded in buffer */
int git_buf_put_base64(git_buf *buf, const char *data, size_t len);

/*
 * Insert, remove or replace a portion of the buffer.
 *
 * @param buf The buffer to work with
 *
 * @param where The location in the buffer where the transformation
 * should be applied.
 *
 * @param nb_to_remove The number of chars to be removed. 0 to not
 * remove any character in the buffer.
 *
 * @param data A pointer to the data which should be inserted.
 *
 * @param nb_to_insert The number of chars to be inserted. 0 to not
 * insert any character from the buffer.
 *
 * @return 0 or an error code.
 */
int git_buf_splice(
	git_buf *buf,
	size_t where,
	size_t nb_to_remove,
	const char *data,
	size_t nb_to_insert);

#endif
