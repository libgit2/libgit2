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
#include "git2/config.h"

/* Tweaked from Core Git. I wonder what we could use this for... */
void git_text_gather_stats(git_text_stats *stats, const git_buf *text)
{
	size_t i;

	memset(stats, 0, sizeof(*stats));

	for (i = 0; i < git_buf_len(text); i++) {
		unsigned char c = text->ptr[i];

		if (c == '\r') {
			stats->cr++;

			if (i + 1 < git_buf_len(text) && text->ptr[i + 1] == '\n')
				stats->crlf++;
		}

		else if (c == '\n')
			stats->lf++;

		else if (c == 0x85)
			/* Unicode CR+LF */
			stats->crlf++;

		else if (c == 127)
			/* DEL */
			stats->nonprintable++;

		else if (c <= 0x1F || (c >= 0x80 && c <= 0x9F)) {
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
	if (git_buf_len(text) >= 1 && text->ptr[text->size - 1] == '\032')
		stats->nonprintable--;
}

/*
 * Fresh from Core Git
 */
int git_text_is_binary(git_text_stats *stats)
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

int git_filters_load(git_vector *filters, git_repository *repo, const char *path, int mode)
{
	int error;

	if (mode == GIT_FILTER_TO_ODB) {
		/* Load the CRLF cleanup filter when writing to the ODB */
		error = git_filter_add__crlf_to_odb(filters, repo, path);
		if (error < 0)
			return error;
	} else {
		giterr_set(GITERR_INVALID, "Worktree filters are not implemented yet");
		return -1;
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

