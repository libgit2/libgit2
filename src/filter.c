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

/* Fresh from Core Git. I wonder what we could use this for... */
void git_text__stat(git_text_stats *stats, const git_buf *text)
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

int git_filter__load_for_file(git_vector *filters, git_repository *repo, const char *path, int mode)
{
	int error;
	git_filter *crlf_filter;

	return 0; /* TODO: not quite ready yet */

	if (mode == GIT_FILTER_TO_ODB) {
		error = git_filter__crlf_to_odb(&crlf_filter, repo, path);
		if (error < GIT_SUCCESS)
			return error;

		if (crlf_filter != NULL)
			git_vector_insert(filters, crlf_filter);

	} else {
		return git__throw(GIT_ENOTIMPLEMENTED,
			"Worktree filters are not implemented yet");
	}

	return 0;
}

void git_filter__free(git_vector *filters)
{
	size_t i;
	git_filter *filter;

	git_vector_foreach(filters, i, filter) {
		if (filter->do_free != NULL)
			filter->do_free(filter);
		else
			free(filter);
	}

	git_vector_free(filters);
}

int git_filter__apply(git_buf *dest, git_buf *source, git_vector *filters)
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
		git_filter *filter = git_vector_get(filters, i);
		dst = (src + 1) % 2;

		git_buf_clear(dbuffer[dst]);

		/* Apply the filter, from dbuffer[src] to dbuffer[dst];
		 * if the filtering is canceled by the user mid-filter,
		 * we skip to the next filter without changing the source
		 * of the double buffering (so that the text goes through
		 * cleanly).
		 */
		if (filter->apply(filter, dbuffer[dst], dbuffer[src]) == 0) {
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

