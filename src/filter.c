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
#include "blob.h"
#include "git2/config.h"
#include "git2/sys/filter.h"

#define filter_foreach(v, mode, iter, elem) \
	for ((iter) = ((mode) == GIT_FILTER_TO_ODB) ? 0 : (v)->length - 1; \
		(iter) < (((mode) == GIT_FILTER_TO_ODB) ? (v)->length : SIZE_MAX) && ((elem) = (v)->contents[(iter)], 1); \
		(iter) = ((mode) == GIT_FILTER_TO_ODB) ? (iter) + 1 : (iter) - 1)

typedef struct
{
	git_filter *filter;
	int priority;
} filter_internal;

static int filter_cmp(const void *a, const void *b)
{
	const filter_internal *f_a = a;
	const filter_internal *f_b = b;

	if (f_a->priority < f_b->priority)
		return -1;
	else if (f_a->priority > f_b->priority)
		return 1;

	return 0;
}

int git_filters__init(git_vector *filters)
{
	filters->_cmp = filter_cmp;
	return 0;
}

int git_filters__add(git_vector *filters, git_filter *filter, int priority)
{
	filter_internal *f;

	if ((f = git__calloc(1, sizeof(filter_internal))) == NULL)
		return -1;

	f->filter = filter;
	f->priority = priority;

	if (git_vector_insert(filters, f) < 0) {
		git__free(f);
		return -1;
	}

	git_vector_sort(filters);

	return 0;
}

int git_filters__load(
	git_vector *filters,
	git_repository *repo,
	const char *path,
	git_filter_mode_t mode)
{
	filter_internal *f;
	size_t i;
	int error, count = 0;

	filter_foreach(&repo->filters, mode, i, f) {
		if (f->filter->should_apply(f->filter, path, mode)) {
			if ((error = git_vector_insert(filters, f)) < 0)
				return error;
			
			++count;
		}
	}

	return count;
}

int git_filters__apply(
	git_filterbuf **out,
	git_vector *filters,
	const char *path,
	git_filter_mode_t mode,
	const void *src,
	size_t src_len)
{
	filter_internal *f;
	void *cur;
	void *dst = NULL;
	size_t cur_len, dst_len = 0;
	void (*cur_free)(void *);
	size_t i;
	int error = 0;
	int filtered = 0;

	*out = NULL;

	if (src_len == 0)
		return 0;

	cur = (void *)src;
	cur_len = src_len;

	filter_foreach(filters, mode, i, f) {
		if ((error = f->filter->apply(&dst, &dst_len, f->filter, path, mode, cur, cur_len)) < 0)
			return error;

		/* Filter cancelled application; do nothing. */
		if (error == 0)
			continue;

		if (filtered)
			cur_free(cur);

		filtered++;

		cur = dst;
		cur_len = dst_len;
		cur_free = f->filter->free_buf;
	}

	if (filtered) {
		if ((*out = git__calloc(1, sizeof(git_filterbuf))) == NULL)
			return -1;

		(*out)->ptr = cur;
		(*out)->len = cur_len;
		(*out)->free = cur_free;
	}

	return filtered;
}

void git_filters__free(git_vector *filters)
{
	filter_internal *f;
	size_t i;

	git_vector_foreach(filters, i, f) {
		if (f->filter->free)
			f->filter->free(f->filter);

		git__free(f);
	}

	git_vector_free(filters);
}
