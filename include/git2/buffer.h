/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_buf_h__
#define INCLUDE_git_buf_h__

#include "common.h"
#include "types.h"

/**
 * @file git2/buffer.h
 * @brief Buffer export structure
 *
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * Static initializer for git_buf from static buffer
 */
#define GIT_BUF_INIT_CONST(STR,LEN) { (char *)(STR), 0, (size_t)(LEN) }

/**
 * Free the memory referred to by the git_buf.
 *
 * Note that this does not free the `git_buf` itself, just the memory
 * referenced by the buffer.  This will not free the memory if it looks
 * like it was not allocated internally, but it will clear the buffer back
 * to the empty state.
 *
 * @param buffer The buffer to deallocate
 */
GIT_EXTERN(void) git_buf_free(git_buf *buffer);

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
GIT_EXTERN(int) git_buf_grow(git_buf *buffer, size_t target_size);

/**
 * Set buffer to a copy of some raw data.
 *
 * @param buffer The buffer to set
 * @param data The data to copy into the buffer
 * @param datalen The length of the data to copy into the buffer
 * @return 0 on success, -1 on allocation failure
 */
GIT_EXTERN(int) git_buf_set(
	git_buf *buffer, const void *data, size_t datalen);

/**
* Check quickly if buffer looks like it contains binary data
*
* @param buf Buffer to check
* @return 1 if buffer looks like non-text data
*/
GIT_EXTERN(int) git_buf_is_binary(const git_buf *buf);

/**
* Check quickly if buffer contains a NUL byte
*
* @param buf Buffer to check
* @return 1 if buffer contains a NUL byte
*/
GIT_EXTERN(int) git_buf_contains_nul(const git_buf *buf);

/**
* Read data contained in the buffer
*
* @param bug Buffer to read
* @param data_out Pointer to the buffer's data
* @param datalen The length of the data in the buffer
* @return 0 if success, otherwise -1
*/
GIT_EXTERN(int) git_buf_read(const git_buf *buffer, const char **data_out, size_t *datalen);

/**
* Gets the length of the buffer.
*
* @param buf Buffer to read
* @return The length of the data contained in the buffer
*/
GIT_EXTERN(size_t) git_buf_len(const git_buf *buffer);

/**
* Gets the length of the buffer.
*
* @param buf Buffer to read
* @return The size of the buffer
*/
GIT_EXTERN(size_t) git_buf_size(const git_buf *buffer);

/**
* Copies a buffer into an array.
*
* @param buffer Buffer to copy
* @param copy_to The array to copy the buffer to
* @param len The length of the array to be copied to
* @param copiedlen_out The number of bytes copued from the buffer into the array
* @return 0 if successful, otherwise -1
*/
GIT_EXTERN(int) git_buf_copy(const git_buf* buffer, char *copy_to, size_t len, size_t *copiedlen_out);

GIT_END_DECL

/** @} */

#endif
