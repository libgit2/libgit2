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

typedef struct {
  const char *key;
  const char *value;
} git_message_trailer;

typedef struct {
  git_message_trailer *trailers;
  size_t count;

  /* private */
  char *trailer_block;
} git_message_trailer_array;

/**
 * Parse trailers out of a message, calling a callback once for each trailer.
 *
 * Return non-zero from the callback to stop processing.
 *
 * Trailers are key/value pairs in the last paragraph of a message, not
 * including any patches or conflicts that may be present.
 *
 * @param arr A pre-allocated git_message_trailer_array struct to be filled in
 *            with any trailers found during parsing.
 * @param message The message to be parsed
 * @return 0 on success, or non-zero callback return value.
 */
GIT_EXTERN(int) git_message_trailers(git_message_trailer_array *arr, const char *message);

GIT_EXTERN(void) git_message_trailer_array_free(git_message_trailer_array *arr);

/** @} */
GIT_END_DECL

#endif /* INCLUDE_git_message_h__ */
