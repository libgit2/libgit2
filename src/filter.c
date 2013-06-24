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
			APPLY_FILTER(GIT_FILTER_TO_WORKDIR, apply_to_workdir)) {
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

void git_filter_free(git_filter *filter) 
{
	if (filter == NULL)
		return;

	git__free(filter->name);
	git__free(filter);
}

int git_filters_create_filter(
	git_filter **out,
	should_apply_to_path_cb should_apply, 
	apply_to_cb apply_to_odb,
	apply_to_cb apply_to_workdir,
	do_free_cb free,
	const char *name)
{
	git_filter *filter;

	if (!name || name[0] == '\0') {
		giterr_set(GITERR_INVALID, "A filter must have a name.");
		return GIT_EINVALIDSPEC;
	}

	filter = git__malloc(sizeof(git_filter));
	GITERR_CHECK_ALLOC(filter);

	filter->should_apply_to_path = should_apply;
	filter->apply_to_odb = apply_to_odb;
	filter->apply_to_workdir = apply_to_workdir;
	filter->do_free = free;

	filter->name = git__strdup(name);
	GITERR_CHECK_ALLOC(filter->name);

	*out = filter;

	return 0;
}

static int find_filter_by_name(const void *a, const void *b)
{
	const git_filter *filter = (const git_filter *)(b);
	const char *key = (const char *)(a);

	return strcmp(key, filter->name);
}

int git_filters_register_filter(git_repository *repo, git_filter *filter)
{
	int error;

	if (!filter->name || filter->name[0] == '\0') {
		giterr_set(GITERR_INVALID, "A filter must have a name.");
		return GIT_EINVALIDSPEC;
	}

	if (git_vector_search2(NULL, &repo->filters, find_filter_by_name,
		filter->name) != GIT_ENOTFOUND)
			return GIT_EEXISTS;

	if ((error = git_vector_insert(&repo->filters, filter)) < 0)
		return error;

	return 0;
}

int git_filters_unregister_filter(git_repository *repo, const char *filtername)
{
	int error;
	size_t pos;
	git_filter *filter;

	if ((error = git_vector_search2(&pos, &repo->filters, find_filter_by_name,
		filtername)) < 0)
			return error;

	filter = (git_filter *)git_vector_get(&repo->filters, pos);

	if (filter->do_free)
		filter->do_free(filter);
	else
		git_filter_free(filter);

	if ((error = git_vector_remove(&repo->filters, pos)) < 0)
		return error;

	return 0;
}
