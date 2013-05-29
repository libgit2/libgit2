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
#include "git2/filter.h"
#include "repository.h"
#include "git2/config.h"
#include "blob.h"

int git_filters__get_filters_to_apply(
	git_vector *filters, git_repository *repo, const char *path,
	git_filter_mode_t mode)
{
	size_t i;
	git_filter *filter;

	git_vector_foreach(&repo->filters, i, filter) {
		if (filter->should_apply_to_path(filter, repo, path, mode)) {
			git_filter_internal *internal_filter;

			internal_filter = git__malloc(sizeof(git_filter_internal));
			GITERR_CHECK_ALLOC(internal_filter);

			internal_filter->filter = filter;
			internal_filter->mode = mode;
			internal_filter->repo = repo;
			internal_filter->path = git__strdup(path);

			git_vector_insert(filters, internal_filter);
		}
	}

	return (int)filters->length;
}

void git_filters__free(git_vector *filters)
{
	size_t i;
	git_filter_internal *internal_filter;

	git_vector_foreach(filters, i, internal_filter) {
		git__free(internal_filter->path);
		git__free(internal_filter);
	}

	git_vector_free(filters);
}

#define APPLY_FILTER(DIRECTION, APPLY) ( \
		internal_filter->mode == DIRECTION && \
		internal_filter->filter->APPLY != NULL && \
		internal_filter->filter->APPLY(internal_filter->filter, \
		internal_filter->repo, internal_filter->path, \
		current_source, current_source_size, \
		&filtered_output, &filtered_output_size) == 0 \
	)

int git_filters__apply(
	git_buf *dest, git_buf *source, git_vector *filters)
{
	size_t i, current_source_size = git_buf_len(source);
	const char *initial_source = git_buf_cstr(source);
	char *current_source = (char *)initial_source;

	for (i = 0; i < filters->length; ++i) {
		git_filter_internal *internal_filter = git_vector_get(filters, i);
		char *filtered_output;
		size_t filtered_output_size;

		/* Apply the filter, and consider the result as the source for
		 * the next filter.
		 */
		if (APPLY_FILTER(GIT_FILTER_TO_ODB, apply_to_odb) ||
			APPLY_FILTER(GIT_FILTER_TO_WORKTREE, apply_to_worktree)) {
				if (current_source != initial_source)
					git__free(current_source);

				current_source = filtered_output;
				current_source_size = filtered_output_size;
				filtered_output = NULL;
		}
	}

	if (current_source == source->ptr)
		git_buf_swap(dest, source);
	else {
		git_buf_attach(dest, current_source, current_source_size, 0);
	}

	return 0;
}
