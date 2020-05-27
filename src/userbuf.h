/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_userbuf_h__
#define INCLUDE_userbuf_h__

#include "git2/userbuf.h"
#include "buffer.h"

/**
 * Sanitizes git_userbuf structures provided from user input, zeroing
 * the data.
 */
extern void git_userbuf_sanitize(git_userbuf *buf);

/**
 * Clears the user buffer.
 */
GIT_INLINE(void) git_userbuf_clear(git_userbuf *buf)
{
	git_buf_clear((git_buf *)buf);
}

#endif
