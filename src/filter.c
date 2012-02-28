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

#include "git2/attr.h"

/* Fresh from Core Git. I wonder what we could use this for... */
void git_text__stat(git_text_stats *stats, git_buf *text)
{
	size_t i;

	memset(stats, 0, sizeof(*stats));

	for (i = 0; i < text->size; i++) {
		unsigned char c = text->ptr[i];

		if (c == '\r') {
			stats->cr++;

			if (i + 1 < text->size && text->ptr[i + 1] == '\n')
				stats->crlf++;

			continue;
		}

		if (c == '\n') {
			stats->lf++;
			continue;
		}

		if (c == 127)
			/* DEL */
			stats->nonprintable++;

		else if (c < 32) {
			switch (c) {
				/* BS, HT, ESC and FF */
			case '\b': case '\t': case '\033': case '\014':
				stats->printable++;
				break;
			case 0:
				stats->nul++;
				/* fall through */
			default:
				stats->nonprintable++;
			}
		}
		else
			stats->printable++;
	}

	/* If file ends with EOF then don't count this EOF as non-printable. */
	if (text->size >= 1 && text->ptr[text->size - 1] == '\032')
		stats->nonprintable--;
}

/*
 * Fresh from Core Git
 */
int git_text__is_binary(git_text_stats *stats)
{
	if (stats->nul)
		return 1;

	if ((stats->printable >> 7) < stats->nonprintable)
		return 1;
	/*
	 * Other heuristics? Average line length might be relevant,
	 * as might LF vs CR vs CRLF counts..
	 *
	 * NOTE! It might be normal to have a low ratio of CRLF to LF
	 * (somebody starts with a LF-only file and edits it with an editor
	 * that adds CRLF only to lines that are added..). But do  we
	 * want to support CR-only? Probably not.
	 */
	return 0;
}

int git_filter__load_for_file(git_vector *filters, git_repository *repo, const char *full_path, int mode)
{
	/* We don't load any filters yet. HAHA */
	return 0;
}

int git_filter__apply(git_buf *dest, git_buf *source, git_vector *filters, const char *filename)
{
	unsigned int src, dst, i;
	git_buf *dbuffer[2];

	dbuffer[0] = source;
	dbuffer[1] = dest;

	src = 0;

	/* Pre-grow the destination buffer to more or less the size
	 * we expect it to have */
	if (git_buf_grow(dest, source->size) < 0)
		return GIT_ENOMEM;

	for (i = 0; i < filters->length; ++i) {
		git_filter_cb filter = git_vector_get(filters, i);
		dst = (src + 1) % 2;

		git_buf_clear(dbuffer[dst]);

		/* Apply the filter, from dbuffer[src] to dbuffer[dst];
		 * if the filtering is canceled by the user mid-filter,
		 * we skip to the next filter without changing the source
		 * of the double buffering (so that the text goes through
		 * cleanly).
		 */
		if (filter(dbuffer[dst], dbuffer[src], filename) == 0) {
			src = (src + 1) % 2;
		}

		if (git_buf_oom(dbuffer[dst]))
			return GIT_ENOMEM;
	}

	/* Ensure that the output ends up in dbuffer[1] (i.e. the dest) */
	if (dst != 1) {
		git_buf_swap(dest, source);
	}

	return GIT_SUCCESS;
}


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

static int check_ident(const char *value)
{
	return (value == git_attr__true);
}

#if 0
static int input_crlf_action(enum crlf_action text_attr, enum eol eol_attr)
{
	if (text_attr == CRLF_BINARY)
		return CRLF_BINARY;
	if (eol_attr == EOL_LF)
		return CRLF_INPUT;
	if (eol_attr == EOL_CRLF)
		return CRLF_CRLF;
	return text_attr;
}
#endif

int git_filter__load_attrs(git_conv_attrs *ca, git_repository *repo, const char *path)
{
#define NUM_CONV_ATTRS 5

	static const char *attr_names[NUM_CONV_ATTRS] = {
		"crlf", "ident", "filter", "eol", "text",
	};

	const char *attr_vals[NUM_CONV_ATTRS];
	int error;

	error = git_attr_get_many(repo, path, NUM_CONV_ATTRS, attr_names, attr_vals);

	if (error == GIT_ENOTFOUND) {
		ca->crlf_action = GIT_CRLF_GUESS;
		ca->eol_attr = GIT_EOL_UNSET;
		ca->ident = 0;
		return 0;
	}

	if (error == GIT_SUCCESS) {
		ca->crlf_action = check_crlf(attr_vals[4]); /* text */
		if (ca->crlf_action == GIT_CRLF_GUESS)
			ca->crlf_action = check_crlf(attr_vals[0]); /* clrf */

		ca->ident = check_ident(attr_vals[1]); /* ident */
		ca->eol_attr = check_eol(attr_vals[3]); /* eol */
		return 0;
	}

	return error;
}
