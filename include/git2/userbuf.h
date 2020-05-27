/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_userbuf_h__
#define INCLUDE_git_userbuf_h__

#include "common.h"

/**
 * @file git2/userbuf.h
 * @brief Buffer export structure
 *
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * An optional initialization macro for `git_userbuf` objects.
 */
#define GIT_USERBUF_INIT { NULL, 0, 0 }

/**
 * A data buffer for exporting data from libgit2.
 *
 * Sometimes libgit2 wants to return an allocated data buffer to the
 * caller and have the caller take responsibility for freeing that memory.
 * This structure should be freed with `git_userbuf_dispose` when you
 * have finished with it.
 */
typedef struct {
	/**
	 * The buffer's contents.  This is a NUL terminated C string.
	 */
	char *ptr;

	/**
	 * This is the allocated size of the allocated buffer.  For
	 * buffers returned to you from libgit2, you should not
	 * modify this value.  For any buffer that you pass to
	 * libgit2, this remain 0.
	 */
	size_t asize;

	/**
	 * The size (in bytes) of the data in the buffer, not including
	 * the NUL terminating character.
	 */
	size_t size;
} git_userbuf;

/**
 * Places the given data in the buffer.  This is necessary for some
 * callback functions that take user data.  If there is already data in
 * the buffer, you should call `git_userbuf_dispose` before setting the
 * buffer data.
 *
 * @param buf The buffer to place data into
 * @param ptr The data to place in the buffer
 * @param len The length of the data in bytes
 */
GIT_EXTERN(int) git_userbuf_set(git_userbuf *buf, const char *ptr, size_t len);

/**
 * Free the memory referred to by the git_userbuf.
 *
 * @param buf The buffer to deallocate
 */
GIT_EXTERN(void) git_userbuf_dispose(git_userbuf *buf);

GIT_END_DECL

/** @} */

#endif
