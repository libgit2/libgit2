/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_message_h__
#define INCLUDE_git_message_h__

#include "common.h"
#include "buffer.h"

/**
 * @file git2/message.h
 * @brief Git message management routines
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * Clean up message from excess whitespace and make sure that the last line
 * ends with a '\n'.
 *
 * Optionally, can remove lines starting with a "#".
 *
 * @param out The user-allocated git_buf which will be filled with the
 *     cleaned up message.
 *
 * @param message The message to be prettified.
 *
 * @param strip_comments Non-zero to remove comment lines, 0 to leave them in.
 *
 * @param comment_char Comment character. Lines starting with this character
 * are considered to be comments and removed if `strip_comments` is non-zero.
 *
 * @return 0 or an error code.
 */
GIT_EXTERN(int) git_message_prettify(git_buf *out, const char *message, int strip_comments, char comment_char);

typedef int(*git_message_trailer_cb)(const char *key, const char *value, void *payload);

/**
 * Parse trailers out of a message, calling a callback once for each trailer.
 *
 * Return non-zero from the callback to stop processing.
 *
 * Trailers are key/value pairs in the last paragraph of a message, not
 * including any patches or conflicts that may be present.
 *
 * @param message The message to be parsed
 * @param cb The callback to call for each trailer found in the message. The
 *     key and value arguments are pointers to NUL-terminated C strings. These
 *     pointers are only guaranteed to be valid until the callback returns.
 *     User code must make a copy of this data should it need to be retained
 * @param payload Pointer to callback data (optional)
 * @return 0 on success, or non-zero callback return value.
 */
GIT_EXTERN(int) git_message_trailers(const char *message, git_message_trailer_cb cb, void *payload);

typedef struct git_message_trailer_iterator git_message_trailer_iterator;

GIT_EXTERN(int) git_message_trailer_iterator_new(
	git_message_trailer_iterator **out,
	const char *message);

GIT_EXTERN(int) git_message_trailer_iterator_next(
	const char **key_out,
	const char **value_out,
	git_message_trailer_iterator *iter);

GIT_EXTERN(void) git_message_trailer_iterator_free(git_message_trailer_iterator *iter);

/** @} */
GIT_END_DECL

#endif /* INCLUDE_git_message_h__ */
