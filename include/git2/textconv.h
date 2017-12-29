/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_textconv_h__
#define INCLUDE_git_textconv_h__

#include "common.h"
#include "types.h"
#include "oid.h"
#include "buffer.h"

/**
 * @file git2/textconv.h
 * @brief Git textconv APIs
 *
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL


/**
 * A textconv that can transform file data
 *
 * This represents a textconv that can be used to transform or even replace
 * file data. Libgit2 includes one built in textconv and it is possible to
 * write your own (see git2/sys/textconv.h for information on that).
 */
typedef struct git_textconv git_textconv;


/**
 * Load the textconv for a given path.
 *
 * This will return 0 (success) but set the output git_textconv to NULL
 * if no textconvs are requested for the given file.
 *
 * @param textconv Output reference to a git_textconv (or NULL)
 * @param repo Repository object that contains `path`
 * @param path Relative path of the file to be textconved
 * @return 0 on success (which could still return NULL if no textconvs are
 *         needed for the requested file), <0 on error
 */
GIT_EXTERN(int) git_textconv_load(
                                  git_textconv **textconv,
                                  git_repository *repo,
                                  const char *path);



/**
 * Apply textconv to a data buffer.
 *
 * See `git2/buffer.h` for background on `git_buf` objects.
 *
 * If the `in` buffer holds data allocated by libgit2 (i.e. `in->asize` is
 * not zero), then it will be overwritten when applying the textconv.  If
 * not, then it will be left untouched.
 *
 * If `textconv` is NULL then the `out` buffer will reference the `in`
 * buffer data (with `asize` set to zero) instead of allocating data.
 * This keeps allocations to a minimum, but it means you have to be careful
 * about freeing the `in` data since `out` may be pointing to it!
 *
 * @param out Buffer to store the result of the textconving
 * @param textconv A loaded git_textconv (or NULL)
 * @param in Buffer containing the data to textconv
 * @return 0 on success, an error code otherwise
 */
GIT_EXTERN(int) git_textconv_apply_to_data(
                                           git_buf *out,
                                           git_textconv *textconv,
                                           git_buf *in);

/**
 * Apply a textconv to the contents of a file on disk
 *
 * @param out buffer into which to store the textconved file
 * @param textconv the textconv to apply
 * @param repo the repository in which to perform the textconving
 * @param path the path of the file to textconv, a relative path will be
 * taken as relative to the workdir
 */
GIT_EXTERN(int) git_textconv_apply_to_file(
                                           git_buf *out,
                                           git_textconv *textconv,
                                           git_repository *repo,
                                           const char *path);

/**
 * Apply a textconv to the contents of a blob
 *
 * @param out buffer into which to store the textconved file
 * @param textconv the textconv to apply
 * @param blob the blob to textconv
 */
GIT_EXTERN(int) git_textconv_apply_to_blob(
                                           git_buf *out,
                                           git_textconv *textconv,
                                           git_blob *blob);

/**
 * Apply a textconv to an arbitrary buffer as a stream
 *
 * @param textconv the textconv to apply
 * @param data the buffer to textconv
 * @param target the stream into which the data will be written
 */
GIT_EXTERN(int) git_textconv_stream_data(
                                         git_textconv *textconv,
                                         git_buf *data,
                                         git_writestream *target);

/**
 * Apply a textconv to a file as a stream
 *
 * @param textconv the textconv to apply
 * @param repo the repository in which to perform the textconving
 * @param path the path of the file to textconv, a relative path will be
 * taken as relative to the workdir
 * @param target the stream into which the data will be written
 */
GIT_EXTERN(int) git_textconv_stream_file(
                                              git_textconv *textconv,
                                              git_repository *repo,
                                              const char *path,
                                              git_writestream *target);

/**
 * Apply a textconv to a blob as a stream
 *
 * @param textconv the textconv to apply
 * @param blob the blob to textconv
 * @param target the stream into which the data will be written
 */
GIT_EXTERN(int) git_textconv_stream_blob(
                                              git_textconv *textconv,
                                              git_blob *blob,
                                              git_writestream *target);

/**
 * Free a git_textconv_list
 *
 * @param textconv A git_textconv created by `git_textconv_load`
 */
GIT_EXTERN(void) git_textconv_free(git_textconv *textconv);


GIT_END_DECL

/** @} */

#endif
