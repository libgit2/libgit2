/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "userbuf.h"
#include "buffer.h"
#include "buf_text.h"
#include <ctype.h>

void git_userbuf_sanitize(git_userbuf *buf)
{
	if (buf->ptr == NULL) {
		buf->ptr = git_buf__initbuf;
		buf->size = 0;
		buf->asize = 0;
	} else if (buf->asize > buf->size) {
		buf->ptr[buf->size] = '\0';
	}
}

int git_userbuf_is_binary(const git_userbuf *buf)
{
	return git_buf_text_is_binary((git_buf *)buf);
}

int git_userbuf_contains_nul(const git_userbuf *buf)
{
	return git_buf_text_contains_nul((git_buf *)buf);
}

int git_userbuf_set(git_userbuf *buf, const void *ptr, size_t len)
{
	return git_buf_set((git_buf *)buf, ptr, len);
}

int git_userbuf_grow(git_userbuf *buf, size_t size)
{
	return git_buf_grow((git_buf *)buf, size);
}

void git_userbuf_dispose(git_userbuf *buf)
{
	git_buf_dispose((git_buf *)buf);
}
