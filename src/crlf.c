/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "fileops.h"
#include "hash.h"
#include "filter.h"
#include "repository.h"

#include "git2/attr.h"

struct crlf_attrs {
	int crlf_action;
	int eol;
};

struct crlf_filter {
	git_filter f;
	struct crlf_attrs attrs;
};

static int check_crlf(const char *value)
{
	if (value == git_attr__true)
		return GIT_CRLF_TEXT;

	if (value == git_attr__false)
		return GIT_CRLF_BINARY;

	if (value == NULL)
		return GIT_CRLF_GUESS;

	if (strcmp(value, "input") == 0)
		return GIT_CRLF_INPUT;

	if (strcmp(value, "auto") == 0)
		return GIT_CRLF_AUTO;

	return GIT_CRLF_GUESS;
}

static int check_eol(const char *value)
{
	if (value == NULL)
		return GIT_EOL_UNSET;

	if (strcmp(value, "lf") == 0)
		return GIT_EOL_LF;

	if (strcmp(value, "crlf") == 0)
		return GIT_EOL_CRLF;

	return GIT_EOL_UNSET;
}

static int crlf_input_action(struct crlf_attrs *ca)
{
	if (ca->crlf_action == GIT_CRLF_BINARY)
		return GIT_CRLF_BINARY;

	if (ca->eol == GIT_EOL_LF)
		return GIT_CRLF_INPUT;

	if (ca->eol == GIT_EOL_CRLF)
		return GIT_CRLF_CRLF;

	return ca->crlf_action;
}

static int crlf_load_attributes(struct crlf_attrs *ca, git_repository *repo, const char *path)
{
#define NUM_CONV_ATTRS 3

	static const char *attr_names[NUM_CONV_ATTRS] = {
		"crlf", "eol", "text",
	};

	const char *attr_vals[NUM_CONV_ATTRS];
	int error;

	error = git_attr_get_many(repo, path, NUM_CONV_ATTRS, attr_names, attr_vals);

	if (error == GIT_ENOTFOUND) {
		ca->crlf_action = GIT_CRLF_GUESS;
		ca->eol = GIT_EOL_UNSET;
		return 0;
	}

	if (error == GIT_SUCCESS) {
		ca->crlf_action = check_crlf(attr_vals[2]); /* text */
		if (ca->crlf_action == GIT_CRLF_GUESS)
			ca->crlf_action = check_crlf(attr_vals[0]); /* clrf */

		ca->eol = check_eol(attr_vals[1]); /* eol */
		return 0;
	}

	return error;
}

static int crlf_apply_to_odb(git_filter *self, git_buf *dest, const git_buf *source)
{
	size_t i = 0;
	struct crlf_filter *filter = (struct crlf_filter *)self;

	assert(self && dest && source);

	if (filter->attrs.crlf_action == GIT_CRLF_AUTO ||
		filter->attrs.crlf_action == GIT_CRLF_GUESS) {

		git_text_stats stats;
		git_text__stat(&stats, source);

		/*
		 * We're currently not going to even try to convert stuff
		 * that has bare CR characters. Does anybody do that crazy
		 * stuff?
		 */
		if (stats.cr != stats.crlf)
			return -1;

		/*
		 * And add some heuristics for binary vs text, of course...
		 */
		if (git_text__is_binary(&stats))
			return -1;

#if 0
		if (crlf_action == CRLF_GUESS) {
			/*
			 * If the file in the index has any CR in it, do not convert.
			 * This is the new safer autocrlf handling.
			 */
			if (has_cr_in_index(path))
				return 0;
		}
#endif

		if (!stats.cr)
			return -1;
	}

	/* TODO: do not copy anything if there isn't a single CR */
	while (i < source->size) {
		size_t org = i;

		while (i < source->size && source->ptr[i] != '\r')
			i++;

		if (i > org)
			git_buf_put(dest, source->ptr + org, i - org);

		i++;

		if (i >= source->size || source->ptr[i] != '\n') {
			git_buf_putc(dest, '\r');
		}
	}

	return 0;
}

int git_filter__crlf_to_odb(git_filter **filter_out, git_repository *repo, const char *path)
{
	struct crlf_filter filter;
	int error;

	filter.f.apply = &crlf_apply_to_odb;
	filter.f.do_free = NULL;

	if ((error = crlf_load_attributes(&filter.attrs, repo, path)) < 0)
		return error;

	filter.attrs.crlf_action = crlf_input_action(&filter.attrs);

	if (filter.attrs.crlf_action == GIT_CRLF_BINARY)
		return 0;

	if (filter.attrs.crlf_action == GIT_CRLF_GUESS && repo->filter_options.auto_crlf == GIT_AUTO_CRLF_FALSE)
		return 0;

	*filter_out = git__malloc(sizeof(struct crlf_filter));
	if (*filter_out == NULL)
		return GIT_ENOMEM;

	memcpy(*filter_out, &filter, sizeof(struct crlf_attrs));
	return 0;
}

