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
	if (GIT_ATTR_TRUE(value))
		return GIT_CRLF_TEXT;

	if (GIT_ATTR_FALSE(value))
		return GIT_CRLF_BINARY;

	if (GIT_ATTR_UNSPECIFIED(value))
		return GIT_CRLF_GUESS;

	if (strcmp(value, "input") == 0)
		return GIT_CRLF_INPUT;

	if (strcmp(value, "auto") == 0)
		return GIT_CRLF_AUTO;

	return GIT_CRLF_GUESS;
}

static int check_eol(const char *value)
{
	if (GIT_ATTR_UNSPECIFIED(value))
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

	error = git_attr_get_many(attr_vals,
		repo, 0, path, NUM_CONV_ATTRS, attr_names);

	if (error == GIT_ENOTFOUND) {
		ca->crlf_action = GIT_CRLF_GUESS;
		ca->eol = GIT_EOL_UNSET;
		return 0;
	}

	if (error == 0) {
		ca->crlf_action = check_crlf(attr_vals[2]); /* text */
		if (ca->crlf_action == GIT_CRLF_GUESS)
			ca->crlf_action = check_crlf(attr_vals[0]); /* clrf */

		ca->eol = check_eol(attr_vals[1]); /* eol */
		return 0;
	}

	return -1;
}

static int drop_crlf(git_buf *dest, const git_buf *source)
{
	const char *scan = source->ptr, *next;
	const char *scan_end = git_buf_cstr(source) + git_buf_len(source);

	/* Main scan loop.  Find the next carriage return and copy the
	 * whole chunk up to that point to the destination buffer.
	 */
	while ((next = memchr(scan, '\r', scan_end - scan)) != NULL) {
		/* copy input up to \r */
		if (next > scan)
			git_buf_put(dest, scan, next - scan);

		/* Do not drop \r unless it is followed by \n */
		if (*(next + 1) != '\n')
			git_buf_putc(dest, '\r');

		scan = next + 1;
	}

	/* If there was no \r, then tell the library to skip this filter */
	if (scan == source->ptr)
		return -1;

	/* Copy remaining input into dest */
	git_buf_put(dest, scan, scan_end - scan);
	return 0;
}

static int crlf_apply_to_odb(git_filter *self, git_buf *dest, const git_buf *source)
{
	struct crlf_filter *filter = (struct crlf_filter *)self;

	assert(self && dest && source);

	/* Empty file? Nothing to do */
	if (git_buf_len(source) == 0)
		return 0;

	/* Heuristics to see if we can skip the conversion.
	 * Straight from Core Git.
	 */
	if (filter->attrs.crlf_action == GIT_CRLF_AUTO ||
		filter->attrs.crlf_action == GIT_CRLF_GUESS) {

		git_text_stats stats;
		git_text_gather_stats(&stats, source);

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
		if (git_text_is_binary(&stats))
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

	/* Actually drop the carriage returns */
	return drop_crlf(dest, source);
}

int git_filter_add__crlf_to_odb(git_vector *filters, git_repository *repo, const char *path)
{
	struct crlf_attrs ca;
	struct crlf_filter *filter;
	int error;

	/* Load gitattributes for the path */
	if ((error = crlf_load_attributes(&ca, repo, path)) < 0)
		return error;

	/*
	 * Use the core Git logic to see if we should perform CRLF for this file
	 * based on its attributes & the value of `core.auto_crlf`
	 */
	ca.crlf_action = crlf_input_action(&ca);

	if (ca.crlf_action == GIT_CRLF_BINARY)
		return 0;

	if (ca.crlf_action == GIT_CRLF_GUESS) {
		int auto_crlf;

		if ((error = git_repository__cvar(
			&auto_crlf, repo, GIT_CVAR_AUTO_CRLF)) < 0)
			return error;

		if (auto_crlf == GIT_AUTO_CRLF_FALSE)
			return 0;
	}

	/* If we're good, we create a new filter object and push it
	 * into the filters array */
	filter = git__malloc(sizeof(struct crlf_filter));
	GITERR_CHECK_ALLOC(filter);

	filter->f.apply = &crlf_apply_to_odb;
	filter->f.do_free = NULL;
	memcpy(&filter->attrs, &ca, sizeof(struct crlf_attrs));

	return git_vector_insert(filters, filter);
}

