/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#include "buffer.h"
#include "posix.h"
#include <stdarg.h>

/* Used as default value for git_buf->ptr so that people can always
 * assume ptr is non-NULL and zero terminated even for new git_bufs.
 */
char git_buf_initbuf[1];

#define ENSURE_SIZE(b, d) \
	if ((ssize_t)(d) > buf->asize && git_buf_grow(b, (d)) < GIT_SUCCESS)\
		return;

void git_buf_init(git_buf *buf, size_t initial_size)
{
	buf->asize = 0;
	buf->size = 0;
	buf->ptr = git_buf_initbuf;

	if (initial_size)
		git_buf_grow(buf, initial_size);
}

int git_buf_grow(git_buf *buf, size_t target_size)
{
	char *new_ptr;
	size_t new_size;

	if (buf->asize < 0)
		return GIT_ENOMEM;

	if (target_size <= (size_t)buf->asize)
		return GIT_SUCCESS;

	if (buf->asize == 0) {
		new_size = target_size;
		new_ptr = NULL;
	} else {
		new_size = (size_t)buf->asize;
		new_ptr = buf->ptr;
	}

	/* grow the buffer size by 1.5, until it's big enough
	 * to fit our target size */
	while (new_size < target_size)
		new_size = (new_size << 1) - (new_size >> 1);

	/* round allocation up to multiple of 8 */
	new_size = (new_size + 7) & ~7;

	new_ptr = git__realloc(new_ptr, new_size);
	if (!new_ptr) {
		buf->asize = -1;
		return GIT_ENOMEM;
	}

	buf->asize = new_size;
	buf->ptr   = new_ptr;

	/* truncate the existing buffer size if necessary */
	if (buf->size >= buf->asize)
		buf->size = buf->asize - 1;
	buf->ptr[buf->size] = '\0';

	return GIT_SUCCESS;
}

void git_buf_free(git_buf *buf)
{
	if (!buf) return;

	if (buf->ptr != git_buf_initbuf)
		git__free(buf->ptr);

	git_buf_init(buf, 0);
}

void git_buf_clear(git_buf *buf)
{
	buf->size = 0;
	if (buf->asize > 0)
		buf->ptr[0] = '\0';
}

int git_buf_oom(const git_buf *buf)
{
	return (buf->asize < 0);
}

void git_buf_set(git_buf *buf, const char *data, size_t len)
{
	if (len == 0 || data == NULL) {
		git_buf_clear(buf);
	} else {
		ENSURE_SIZE(buf, len + 1);
		memmove(buf->ptr, data, len);
		buf->size = len;
		buf->ptr[buf->size] = '\0';
	}
}

void git_buf_sets(git_buf *buf, const char *string)
{
	git_buf_set(buf, string, string ? strlen(string) : 0);
}

void git_buf_putc(git_buf *buf, char c)
{
	ENSURE_SIZE(buf, buf->size + 2);
	buf->ptr[buf->size++] = c;
	buf->ptr[buf->size] = '\0';
}

void git_buf_put(git_buf *buf, const char *data, size_t len)
{
	ENSURE_SIZE(buf, buf->size + len + 1);
	memmove(buf->ptr + buf->size, data, len);
	buf->size += len;
	buf->ptr[buf->size] = '\0';
}

void git_buf_puts(git_buf *buf, const char *string)
{
	assert(string);
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
	return buf->ptr;
}

void git_buf_copy_cstr(char *data, size_t datasize, git_buf *buf)
{
	size_t copylen;

	assert(data && datasize);

	data[0] = '\0';

	if (buf->size == 0 || buf->asize <= 0)
		return;

	copylen = buf->size;
	if (copylen > datasize - 1)
		copylen = datasize - 1;
	memmove(data, buf->ptr, copylen);
	data[copylen] = '\0';
}

void git_buf_consume(git_buf *buf, const char *end)
{
	if (end > buf->ptr && end <= buf->ptr + buf->size) {
		size_t consumed = end - buf->ptr;
		memmove(buf->ptr, end, buf->size - consumed);
		buf->size -= consumed;
		buf->ptr[buf->size] = '\0';
	}
}

void git_buf_swap(git_buf *buf_a, git_buf *buf_b)
{
	git_buf t = *buf_a;
	*buf_a = *buf_b;
	*buf_b = t;
}

char *git_buf_take_cstr(git_buf *buf)
{
	char *data = buf->ptr;

	if (buf->asize <= 0)
		return NULL;

	git_buf_init(buf, 0);

	return data;
}

void git_buf_join_n(git_buf *buf, char separator, int nbuf, ...)
{
	/* Make two passes to avoid multiple reallocation */

	va_list ap;
	int i;
	size_t total_size = 0;
	char *out;

	if (buf->size > 0 && buf->ptr[buf->size - 1] != separator)
		++total_size; /* space for initial separator */

	va_start(ap, nbuf);
	for (i = 0; i < nbuf; ++i) {
		const char* segment;
		size_t segment_len;

		segment = va_arg(ap, const char *);
		if (!segment)
			continue;

		segment_len = strlen(segment);
		total_size += segment_len;
		if (segment_len == 0 || segment[segment_len - 1] != separator)
			++total_size; /* space for separator */
	}
	va_end(ap);

	ENSURE_SIZE(buf, buf->size + total_size + 1);

	out = buf->ptr + buf->size;

	/* append separator to existing buf if needed */
	if (buf->size > 0 && out[-1] != separator)
		*out++ = separator;

	va_start(ap, nbuf);
	for (i = 0; i < nbuf; ++i) {
		const char* segment;
		size_t segment_len;

		segment = va_arg(ap, const char *);
		if (!segment)
			continue;

		/* skip leading separators */
		if (out > buf->ptr && out[-1] == separator)
			while (*segment == separator) segment++;

		/* copy over next buffer */
		segment_len = strlen(segment);
		if (segment_len > 0) {
			memmove(out, segment, segment_len);
			out += segment_len;
		}

		/* append trailing separator (except for last item) */
		if (i < nbuf - 1 && out > buf->ptr && out[-1] != separator)
			*out++ = separator;
	}
	va_end(ap);

	/* set size based on num characters actually written */
	buf->size = out - buf->ptr;
	buf->ptr[buf->size] = '\0';
}

void git_buf_join(
	git_buf *buf,
	char separator,
	const char *str_a,
	const char *str_b)
{
	size_t add_size = 0;
	size_t sep_a = 0;
	size_t strlen_a = 0;
	size_t sep_b = 0;
	size_t strlen_b = 0;
	char *ptr;

	/* calculate string lengths and need for added separators */
	if (str_a) {
		while (*str_a == separator) { sep_a = 1; str_a++; }
		strlen_a = strlen(str_a);
	}
	if (str_b) {
		while (*str_b == separator) { sep_b = 1; str_b++; }
		strlen_b = strlen(str_b);
	}
	if (buf->size > 0) {
		if (buf->ptr[buf->size - 1] == separator) {
			sep_a = 0;
			if (!strlen_a) sep_b = 0;
		} else if (!strlen_a)
			sep_b = (str_b != NULL);
	}
	if (strlen_a > 0) {
		if (str_a[strlen_a - 1] == separator)
			sep_b = 0;
		else if (str_b)
			sep_b = 1;
	}

	add_size = sep_a + strlen_a + sep_b + strlen_b;

	if (!add_size) return;

	ENSURE_SIZE(buf, buf->size + add_size + 1);

	/* concatenate strings */
	ptr = buf->ptr + buf->size;
	if (sep_a)
		*ptr++ = separator;
	if (strlen_a) {
		memmove(ptr, str_a, strlen_a);
		ptr += strlen_a;
	}
	if (sep_b)
		*ptr++ = separator;
	if (strlen_b) {
		memmove(ptr, str_b, strlen_b);
		ptr += strlen_b;
	}

	/* set size based on num characters actually written */
	buf->size = ptr - buf->ptr;
	buf->ptr[buf->size] = '\0';
}
