/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#include "buffer.h"
#include "posix.h"
#include <stdarg.h>

#define ENSURE_SIZE(b, d) \
	if ((ssize_t)(d) >= buf->asize && git_buf_grow(b, (d)) < GIT_SUCCESS)\
		return;

int git_buf_grow(git_buf *buf, size_t target_size)
{
	char *new_ptr;

	if (buf->asize < 0)
		return GIT_ENOMEM;

	if (buf->asize == 0)
		buf->asize = target_size;

	/* grow the buffer size by 1.5, until it's big enough
	 * to fit our target size */
	while (buf->asize < (int)target_size)
		buf->asize = (buf->asize << 1) - (buf->asize >> 1);

	new_ptr = git__realloc(buf->ptr, buf->asize);
	if (!new_ptr) {
		buf->asize = -1;
		return GIT_ENOMEM;
	}

	buf->ptr = new_ptr;
	return GIT_SUCCESS;
}

int git_buf_oom(const git_buf *buf)
{
	return (buf->asize < 0);
}

void git_buf_putc(git_buf *buf, char c)
{
	ENSURE_SIZE(buf, buf->size + 1);
	buf->ptr[buf->size++] = c;
}

void git_buf_put(git_buf *buf, const char *data, size_t len)
{
	ENSURE_SIZE(buf, buf->size + len);
	memcpy(buf->ptr + buf->size, data, len);
	buf->size += len;
}

void git_buf_puts(git_buf *buf, const char *string)
{
	git_buf_put(buf, string, strlen(string));
}

void git_buf_printf(git_buf *buf, const char *format, ...)
{
	int len;
	va_list arglist;

	ENSURE_SIZE(buf, buf->size + 1);

	while (1) {
		va_start(arglist, format);
		len = p_vsnprintf(buf->ptr + buf->size, buf->asize - buf->size, format, arglist);
		va_end(arglist);

		if (len < 0) {
			buf->asize = -1;
			return;
		}

		if (len + 1 <= buf->asize - buf->size) {
			buf->size += len;
			return;
		}

		ENSURE_SIZE(buf, buf->size + len + 1);
	}
}

const char *git_buf_cstr(git_buf *buf)
{
	if (buf->size + 1 >= buf->asize && git_buf_grow(buf, buf->size + 1) < GIT_SUCCESS)
		return NULL;

	buf->ptr[buf->size] = '\0';
	return buf->ptr;
}

void git_buf_free(git_buf *buf)
{
	git__free(buf->ptr);
}

void git_buf_clear(git_buf *buf)
{
	buf->size = 0;
}

void git_buf_consume(git_buf *buf, const char *end)
{
	size_t consumed = end - buf->ptr;
	memmove(buf->ptr, end, buf->size - consumed);
	buf->size -= consumed;
}
