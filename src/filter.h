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
#include "git2/odb.h"
#include "git2/repository.h"
#include "git2/sys/filter.h"

typedef struct {
	void *ptr;
	size_t len;

	void (*free)(void *buf);
} git_filterbuf;

int git_filters__init(git_vector *filters);

int git_filters__add(git_vector *filters, git_filter *filter, int priority);

/*
 * For any given path in the working directory, fill the `filters`
 * array with the relevant filters that need to be applied.
 *
 * Mode is either `GIT_FILTER_TO_WORKDIR` if you need to load the
 * filters that will be used when checking out a file to the working
 * directory, or `GIT_FILTER_TO_ODB` for the filters used when writing
 * a file to the ODB.
 *
 * Callers are given a reference to the repository's filters and should
 * not free them.
 *
 * @param filters Vector where to store all the loaded filters
 * @param repo Repository object that contains `path`
 * @param path Relative path of the file to be filtered
 * @param mode Filtering direction (WT->ODB or ODB->WT)
 * @return the number of filters loaded for the file (0 if the file
 *	doesn't need filtering), or a negative error code
 */
int git_filters__load(git_vector *filters, git_repository *repo, const char *path, git_filter_mode_t mode);

/*
 * Apply one or more filters to a file.
 *
 * The resultant filter buf (if filtering occurred) must be freed by calling
 * its free function.
 *
 * @param out Buffer containing the results of the filtering
 * @param filters A non-empty vector of filters as supplied by `git_filters_load`
 * @param path Relative path of the file to be filtered
 * @param src Buffer containing the document to filter
 * @param src_len Length of the document buffer
 * @return number of filters applied (0 if the file was not filtered), or a
 *  negative error code
 */
int git_filters__apply(git_filterbuf **out, git_vector *filters, const char *path, git_filter_mode_t mode, const void *src, size_t src_len);

/**
 * Frees the associated filters.
 */
void git_filters__free(git_vector *filters);

#define GIT_FILTER_CRLF_PRIORITY 1

GIT_INLINE(void) git_filterbuf_free(git_filterbuf *buf)
{
	if (buf == NULL || buf->ptr == NULL)
		return;

	buf->free(buf->ptr);
	git__free(buf);
}

#endif
