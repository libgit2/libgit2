/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "fileops.h"
#include "hash.h"
#include "filter.h"
#include "repository.h"
#include "git2/config.h"
#include "blob.h"

int git_filters_load(git_vector *filters, git_repository *repo, const char *path, int mode)
{
	int error;

	if (mode == GIT_FILTER_TO_ODB) {
		/* Load the CRLF cleanup filter when writing to the ODB */
		error = git_filter_add__crlf_to_odb(filters, repo, path);
		if (error < 0)
			return error;
	} else {
		error = git_filter_add__crlf_to_workdir(filters, repo, path);
		if (error < 0)
			return error;
	}

	return (int)filters->length;
}

void git_filters_free(git_vector *filters)
{
	size_t i;
	git_filter *filter;

	git_vector_foreach(filters, i, filter) {
		if (filter->do_free != NULL)
			filter->do_free(filter);
		else
			git__free(filter);
	}

	git_vector_free(filters);
}

int git_filters_apply(git_buf *dest, git_buf *source, git_vector *filters)
{
	unsigned int i, src;
	git_buf *dbuffer[2];

	dbuffer[0] = source;
	dbuffer[1] = dest;

	src = 0;

	if (git_buf_len(source) == 0) {
		git_buf_clear(dest);
		return 0;
	}

	/* Pre-grow the destination buffer to more or less the size
	 * we expect it to have */
	if (git_buf_grow(dest, git_buf_len(source)) < 0)
		return -1;

	for (i = 0; i < filters->length; ++i) {
		git_filter *filter = git_vector_get(filters, i);
		unsigned int dst = 1 - src;

		git_buf_clear(dbuffer[dst]);

		/* Apply the filter from dbuffer[src] to the other buffer;
		 * if the filtering is canceled by the user mid-filter,
		 * we skip to the next filter without changing the source
		 * of the double buffering (so that the text goes through
		 * cleanly).
		 */
		if (filter->apply(filter, dbuffer[dst], dbuffer[src]) == 0)
			src = dst;

		if (git_buf_oom(dbuffer[dst]))
			return -1;
	}

	/* Ensure that the output ends up in dbuffer[1] (i.e. the dest) */
	if (src != 1)
		git_buf_swap(dest, source);

	return 0;
}
