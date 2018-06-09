/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_filter_textconv_h__
#define INCLUDE_git_filter_textconv_h__

#include "common.h"
#include "types.h"
#include "oid.h"
#include "buffer.h"
#include "filter.h"
#include "textconv.h"


/**
 * Apply filter list and textconv to a data buffer.
 *
 * See `git2/buffer.h` for background on `git_buf` objects.
 *
 * If the `in` buffer holds data allocated by libgit2 (i.e. `in->asize` is
 * not zero), then it will be overwritten when applying the filters.  If
 * not, then it will be left untouched.
 *
 * If there are no filters and no textconv to apply then the `out`
 * buffer will reference the `in` buffer data (with `asize` set to zero)
 * instead of allocating data.  This keeps allocations to a minimum, but
 * it means you have to be careful about freeing the `in` data since `out`
 * may be pointing to it!
 *
 * @param out Buffer to store the result of the filtering
 * @param filters A loaded git_filter_list (or NULL)
 * @param textconv A textconv (or NULL)
 * @param in Buffer containing the data to filter
 * @return 0 on success, an error code otherwise
 */
GIT_EXTERN(int) git_filter_textconv_apply_to_data(
	git_buf *out,
	git_filter_list *filters,
	git_textconv *textconv,
	git_buf *in);

/**
 * Apply a filter list and textconv to the contents of a file on disk
 *
 * @param out buffer into which to store the filtered file
 * @param filters the list of filters to apply
 * @param textconv a textconv to apply
 * @param repo the repository in which to perform the filtering
 * @param path the path of the file to filter, a relative path will be
 * taken as relative to the workdir
 */
GIT_EXTERN(int) git_filter_textconv_apply_to_file(
	git_buf *out,
	git_filter_list *filters,
	git_textconv *textconv,
	git_repository *repo,
	const char *path);

/**
 * Apply a filter list to the contents of a blob
 *
 * @param out buffer into which to store the filtered file
 * @param filters the list of filters to apply
 * @param textconv a textconv to apply
 * @param blob the blob to filter
 */
GIT_EXTERN(int) git_filter_textconv_apply_to_blob(
	git_buf *out,
	git_filter_list *filters,
	git_textconv *textconv,
	git_blob *blob);

/**
 * Apply a filter list to an arbitrary buffer as a stream
 *
 * @param filters the list of filters to apply
 * @param textconv a textconv to apply
 * @param data the buffer to filter
 * @param target the stream into which the data will be written
 */
GIT_EXTERN(int) git_filter_textconv_stream_data(
	git_filter_list *filters,
	git_textconv *textconv,
	git_buf *data,
	git_writestream *target);

/**
 * Apply a filter list to a file as a stream
 *
 * @param filters the list of filters to apply
 * @param textconv a textconv to apply
 * @param repo the repository in which to perform the filtering
 * @param path the path of the file to filter, a relative path will be
 * taken as relative to the workdir
 * @param target the stream into which the data will be written
 */
GIT_EXTERN(int) git_filter_textconv_stream_file(
	git_filter_list *filters,
	git_textconv *textconv,
	git_repository *repo,
	const char *path,
	git_writestream *target);

/**
 * Apply a filter list to a blob as a stream
 *
 * @param filters the list of filters to apply
 * @param textconv a textconv to apply
 * @param blob the blob to filter
 * @param target the stream into which the data will be written
 */
GIT_EXTERN(int) git_filter_textconv_stream_blob(
	git_filter_list *filters,
	git_textconv *textconv,
	git_blob *blob,
	git_writestream *target);

#endif /* INCLUDE_git_filter_h__ */

