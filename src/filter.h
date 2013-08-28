/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_filter_h__
#define INCLUDE_filter_h__

#include "common.h"
#include "buffer.h"
#include "array.h"
#include "git2/odb.h"
#include "git2/repository.h"
#include "git2/filter.h"
#include "git2/sys/filter.h"

typedef enum {
	GIT_CRLF_GUESS = -1,
	GIT_CRLF_BINARY = 0,
	GIT_CRLF_TEXT,
	GIT_CRLF_INPUT,
	GIT_CRLF_CRLF,
	GIT_CRLF_AUTO,
} git_crlf_t;

typedef struct git_filter_list git_filter_list;

/*
 * FILTER API
 */

/*
 * For any given path in the working directory, create a `git_filter_list`
 * with the relevant filters that need to be applied.
 *
 * This will return 0 (success) but set the output git_filter_list to NULL
 * if no filters are requested for the given file.
 *
 * @param filters Output newly created git_filter_list (or NULL)
 * @param repo Repository object that contains `path`
 * @param path Relative path of the file to be filtered
 * @param mode Filtering direction (WT->ODB or ODB->WT)
 * @return 0 on success (which could still return NULL if no filters are
 *         needed for the requested file), <0 on error
 */
extern int git_filter_list_load(
	git_filter_list **filters,
	git_repository *repo,
	const char *path,
	git_filter_mode_t mode);

/*
 * Apply one or more filters to a data buffer.
 *
 * The source data must have been loaded as a `git_buf` object. Both the
 * `source` and `dest` buffers are owned by the caller and must be freed
 * once they are no longer needed.
 *
 * NOTE: Because of the double-buffering schema, the `source` buffer that
 * contains the original file may be tampered once the filtering is
 * complete. Regardless, the `dest` buffer will always contain the final
 * result of the filtering
 *
 * @param dest Buffer to store the result of the filtering
 * @param source Buffer containing the document to filter
 * @param filters An already loaded git_filter_list
 * @return 0 on success, an error code otherwise
 */
extern int git_filter_list_apply(
	git_buf *dest,
	git_buf *source,
	git_filter_list *filters);

/*
 * Free the git_filter_list
 *
 * @param filters A git_filter_list created by `git_filter_list_load`
 */
extern void git_filter_list_free(git_filter_list *filters);

/*
 * Available filters
 */

extern git_filter *git_crlf_filter_new(void);

#endif
