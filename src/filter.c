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

struct git_filter_source {
	git_repository *repo;
	const char     *path;
	git_oid         oid;  /* zero if unknown (which is likely) */
	uint16_t        filemode; /* zero if unknown */
};

typedef struct {
	git_filter *filter;
	void *payload;
} git_filter_entry;

struct git_filter_list {
	git_array_t(git_filter_entry) filters;
	git_filter_mode_t mode;
	git_filter_source source;
	char path[GIT_FLEX_ARRAY];
};

typedef struct {
	const char *filter_name;
	git_filter *filter;
} git_filter_def;

static git_array_t(git_filter_def) filter_registry = GIT_ARRAY_INIT;

git_repository *git_filter_source_repo(const git_filter_source *src)
{
	return src->repo;
}

const char *git_filter_source_path(const git_filter_source *src)
{
	return src->path;
}

uint16_t git_filter_source_filemode(const git_filter_source *src)
{
	return src->filemode;
}

const git_oid *git_filter_source_id(const git_filter_source *src)
{
	return git_oid_iszero(&src->oid) ? NULL : &src->oid;
}

static int filter_load_defaults(void)
{
	if (!git_array_size(filter_registry)) {
		git_filter_def *fdef = git_array_alloc(filter_registry);
		GITERR_CHECK_ALLOC(fdef);

		fdef->filter_name = GIT_FILTER_CRLF;
		fdef->filter = git_crlf_filter_new();
		GITERR_CHECK_ALLOC(fdef->filter);
	}

	return 0;
}

static int git_filter_list_new(
	git_filter_list **out, git_filter_mode_t mode, const git_filter_source *src)
{
	git_filter_list *fl = NULL;
	size_t pathlen = src->path ? strlen(src->path) : 0;

	fl = git__calloc(1, sizeof(git_filter_list) + pathlen + 1);
	GITERR_CHECK_ALLOC(fl);

	fl->mode = mode;
	if (src->path)
		memcpy(fl->path, src->path, pathlen);
	fl->source.repo = src->repo;
	fl->source.path = fl->path;

	*out = fl;
	return 0;
}

int git_filter_list_load(
	git_filter_list **filters,
	git_repository *repo,
	const char *path,
	git_filter_mode_t mode)
{
	int error = 0;
	git_filter_list *fl = NULL;
	git_filter_source src = { 0 };
	git_filter_entry *fe;
	uint32_t f;

	if (filter_load_defaults() < 0)
		return -1;

	src.repo = repo;
	src.path = path;

	for (f = 0; f < git_array_size(filter_registry); ++f) {
		void *payload = NULL;
		git_filter_def *fdef = git_array_get(filter_registry, f);

		if (!fdef || !fdef->filter)
			continue;

		if (fdef->filter->check)
			error = fdef->filter->check(fdef->filter, &payload, mode, &src);

		if (error == GIT_ENOTFOUND)
			error = 0;
		else if (error < 0)
			break;
		else {
			if (!fl && (error = git_filter_list_new(&fl, mode, &src)) < 0)
				return error;

			fe = git_array_alloc(fl->filters);
			GITERR_CHECK_ALLOC(fe);
			fe->filter  = fdef->filter;
			fe->payload = payload;
		}
	}

	if (error && fl != NULL) {
		git_array_clear(fl->filters);
		git__free(fl);
		fl = NULL;
	}

	*filters = fl;
	return error;
}

void git_filter_list_free(git_filter_list *fl)
{
	uint32_t i;

	if (!fl)
		return;

	for (i = 0; i < git_array_size(fl->filters); ++i) {
		git_filter_entry *fe = git_array_get(fl->filters, i);
		if (fe->filter->cleanup)
			fe->filter->cleanup(fe->filter, fe->payload);
	}

	git_array_clear(fl->filters);
	git__free(fl);
}

int git_filter_list_apply(
	git_buf *dest,
	git_buf *source,
	git_filter_list *fl)
{
	int error = 0;
	uint32_t i;
	unsigned int src;
	git_buf *dbuffer[2];

	if (!fl) {
		git_buf_swap(dest, source);
		return 0;
	}

	dbuffer[0] = source;
	dbuffer[1] = dest;

	src = 0;

	/* Pre-grow the destination buffer to more or less the size
	 * we expect it to have */
	if (git_buf_grow(dest, git_buf_len(source)) < 0)
		return -1;

	for (i = 0; i < git_array_size(fl->filters); ++i) {
		git_filter_entry *fe = git_array_get(fl->filters, i);
		unsigned int dst = 1 - src;

		git_buf_clear(dbuffer[dst]);

		/* Apply the filter from dbuffer[src] to the other buffer;
		 * if the filtering is canceled by the user mid-filter,
		 * we skip to the next filter without changing the source
		 * of the double buffering (so that the text goes through
		 * cleanly).
		 */
		{
			git_buffer srcb = GIT_BUFFER_FROM_BUF(dbuffer[src]);
			git_buffer dstb = GIT_BUFFER_FROM_BUF(dbuffer[dst]);

			error = fe->filter->apply(
				fe->filter, &fe->payload, fl->mode, &dstb, &srcb, &fl->source);

			if (error == GIT_ENOTFOUND)
				error = 0;
			else if (error < 0) {
				git_buf_clear(dest);
				return error;
			}
			else {
				git_buf_from_buffer(dbuffer[src], &srcb);
				git_buf_from_buffer(dbuffer[dst], &dstb);
				src = dst;
			}
		}

		if (git_buf_oom(dbuffer[dst]))
			return -1;
	}

	/* Ensure that the output ends up in dbuffer[1] (i.e. the dest) */
	if (src != 1)
		git_buf_swap(dest, source);

	return 0;
}
