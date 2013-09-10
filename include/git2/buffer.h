/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_buffer_h__
#define INCLUDE_git_buffer_h__

#include "common.h"

/**
 * @file git2/buffer.h
 * @brief Buffer export structure
 *
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * A data buffer for exporting data from libgit2
 *
 * Sometimes libgit2 wants to return an allocated data buffer to the
 * caller and have the caller take responsibility for freeing that memory.
 * This can be awkward if the caller does not have easy access to the same
 * allocation functions that libgit2 is using.  In those cases, libgit2
 * will instead fill in a `git_buffer` and the caller can use
 * `git_buffer_free()` to release it when they are done.
 *
 * * `ptr` refers to the start of the allocated memory.
 * * `size` contains the size of the data in `ptr` that is actually used.
 * * `available` refers to the known total amount of allocated memory. It
 *   may be larger than the `size` actually in use.
 *
 * In a few cases, for uniformity and simplicity, an API may populate a
 * `git_buffer` with data that should *not* be freed (i.e. the lifetime of
 * the data buffer is actually tied to another libgit2 object).  These
 * cases will be clearly documented in the APIs that use the `git_buffer`.
 * In those cases, the `available` field will be set to zero even though
 * the `ptr` and `size` will be valid.
 */
typedef struct git_buffer {
	char   *ptr;
	size_t size;
	size_t available;
} git_buffer;

/**
 * Use to initialize buffer structure when git_buffer is on stack
 */
#define GIT_BUFFER_INIT { NULL, 0, 0 }

/**
 * Free the memory referred to by the git_buffer.
 *
 * Note that this does not free the `git_buffer` itself, just the memory
 * pointed to by `buffer->ptr`.  If that memory was not allocated by
 * libgit2 itself, be careful with using this function because it could
 * cause problems.
 *
 * @param buffer The buffer with allocated memory
 */
GIT_EXTERN(void) git_buffer_free(git_buffer *buffer);

/**
 * Resize the buffer allocation to make more space.
 *
 * This will update `buffer->available` with the new size (which will be
 * at least `want_size` and may be larger).  This may or may not change
 * `buffer->ptr` depending on whether there is an existing allocation and
 * whether that allocation can be increased in place.
 *
 * Currently, this will never shrink the buffer, only expand it.
 *
 * @param buffer The buffer to be resized; may or may not be allocated yet
 * @param want_size The desired available size
 * @return 0 on success, negative error code on allocation failure
 */
GIT_EXTERN(int) git_buffer_resize(git_buffer *buffer, size_t want_size);

/**
 * Set buffer to a copy of some raw data.
 *
 * @param buffer The buffer to set
 * @param data The data to copy into the buffer
 * @param datalen The length of the data to copy into the buffer
 * @return 0 on success, negative error code on allocation failure
 */
GIT_EXTERN(int) git_buffer_copy(
	git_buffer *buffer, const void *data, size_t datalen);

GIT_END_DECL

/** @} */

#endif
