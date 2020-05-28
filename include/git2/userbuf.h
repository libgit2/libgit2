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

/** An optional initialization macro for `git_userbuf` objects. */
#define GIT_USERBUF_INIT { NULL, 0, 0 }

/** A constant initialization macro for `git_userbuf` objects. */
#define GIT_USERBUF_CONST(str, len) { (char *)(str), 0, (size_t)(len) }

/**
 * A data buffer for exporting data from libgit2.
 *
 * Sometimes libgit2 wants to return an allocated data buffer to the
 * caller and allow the caller take responsibility for its lifecycle.
 * This requires the caller to free the memory with `git_userbuf_dispose`
 * when they have finished using it.
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
 * Check quickly if buffer looks like it contains binary data
 *
 * @param buf Buffer to check
 * @return 1 if buffer looks like non-text data
 */
GIT_EXTERN(int) git_userbuf_is_binary(const git_userbuf *buf);

 /**
 * Check quickly if buffer contains a NUL byte
 *
 * @param buf Buffer to check
 * @return 1 if buffer contains a NUL byte
 */
GIT_EXTERN(int) git_userbuf_contains_nul(const git_userbuf *buf);

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
GIT_EXTERN(int) git_userbuf_set(git_userbuf *buf, const void *ptr, size_t len);

/**
 * Resize the buffer allocation to make more space.
 *
 * This will attempt to grow the buffer to accommodate the target size.
 *
 * If the buffer refers to memory that was not allocated by libgit2 (i.e.
 * the `asize` field is zero), then `ptr` will be replaced with a newly
 * allocated block of data.  Be careful so that memory allocated by the
 * caller is not lost.  As a special variant, if you pass `target_size` as
 * 0 and the memory is not allocated by libgit2, this will allocate a new
 * buffer of size `size` and copy the external data into it.
 *
 * Currently, this will never shrink a buffer, only expand it.
 *
 * If the allocation fails, this will return an error and the buffer will be
 * marked as invalid for future operations, invaliding the contents.
 *
 * @param buffer The buffer to be resized; may or may not be allocated yet
 * @param target_size The desired available size
 * @return 0 on success, -1 on allocation failure
 */
GIT_EXTERN(int) git_userbuf_grow(git_userbuf *buffer, size_t target_size);

/**
 * Free the memory referred to by the git_userbuf.
 *
 * @param buf The buffer to deallocate
 */
GIT_EXTERN(void) git_userbuf_dispose(git_userbuf *buf);

GIT_END_DECL

/** @} */

#endif
