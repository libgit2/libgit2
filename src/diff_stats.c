/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#include "common.h"
#include "vector.h"
#include "diff.h"
#include "diff_patch.h"

#define DIFF_RENAME_FILE_SEPARATOR " => "

struct git_diff_stats {
	git_vector patches;

	size_t files_changed;
	size_t insertions;
	size_t deletions;
};

static size_t diff_get_filename_padding(
	int has_renames,
	const git_diff_stats *stats)
{
	const git_patch *patch = NULL;
	size_t i, max_padding = 0;

	if (has_renames) {
		git_vector_foreach(&stats->patches, i, patch) {
			const git_diff_delta *delta = NULL;
			size_t len;

			delta = git_patch_get_delta(patch);
			if (strcmp(delta->old_file.path, delta->new_file.path) == 0)
				continue;

			if ((len = strlen(delta->old_file.path) + strlen(delta->new_file.path)) > max_padding)
				max_padding = len;
		}
	}

	git_vector_foreach(&stats->patches, i, patch) {
		const git_diff_delta *delta = NULL;
		size_t len;

		delta = git_patch_get_delta(patch);
		len = strlen(delta->new_file.path);

		if (strcmp(delta->old_file.path, delta->new_file.path) != 0)
			continue;

		if (len > max_padding)
			max_padding = len;
	}

	return max_padding;
}

int git_diff_file_stats__full_to_buf(
	git_buf *out,
	size_t max_padding,
	int has_renames,
	const git_patch *patch)
{
	const char *old_path = NULL, *new_path = NULL;
	const git_diff_delta *delta = NULL;
	size_t padding, old_size, new_size;
	int error;

	delta = git_patch_get_delta(patch);

	old_path = delta->old_file.path;
	new_path = delta->new_file.path;
	old_size = delta->old_file.size;
	new_size = delta->new_file.size;

	if ((error = git_buf_printf(out, " %s", old_path)) < 0)
		goto on_error;

	if (strcmp(old_path, new_path) != 0) {
		padding = max_padding - strlen(old_path) - strlen(new_path);

		if ((error = git_buf_printf(out, DIFF_RENAME_FILE_SEPARATOR "%s", new_path)) < 0)
			goto on_error;
	}
	else {
		padding = max_padding - strlen(old_path);

		if (has_renames)
			padding += strlen(DIFF_RENAME_FILE_SEPARATOR);
	}

	if ((error = git_buf_putcn(out, ' ', padding)) < 0 ||
		(error = git_buf_puts(out, " | ")) < 0)
			goto on_error;

	if (delta->flags & GIT_DIFF_FLAG_BINARY) {
		if ((error = git_buf_printf(out, "Bin %" PRIuZ " -> %" PRIuZ " bytes", old_size, new_size)) < 0)
			goto on_error;
	}
	else {
		size_t insertions, deletions;

		if ((error = git_patch_line_stats(NULL, &insertions, &deletions, patch)) < 0)
			goto on_error;

		if ((error = git_buf_printf(out, "%" PRIuZ, insertions + deletions)) < 0)
			goto on_error;

		if (insertions || deletions) {
			if ((error = git_buf_putc(out, ' ')) < 0 ||
				(error = git_buf_putcn(out, '+', insertions)) < 0 ||
				(error = git_buf_putcn(out, '-', deletions)) < 0)
					goto on_error;
		}
	}

	error = git_buf_putc(out, '\n');

on_error:
	return error;
}

int git_diff_file_stats__number_to_buf(
	git_buf *out,
	const git_patch *patch)
{
	const git_diff_delta *delta = NULL;
	const char *path = NULL;
	size_t insertions, deletions;
	int error;

	delta = git_patch_get_delta(patch);
	path = delta->new_file.path;

	if ((error = git_patch_line_stats(NULL, &insertions, &deletions, patch)) < 0)
		return error;

	if (delta->flags & GIT_DIFF_FLAG_BINARY)
		error = git_buf_printf(out, "%-8c" "%-8c" "%s\n", '-', '-', path);
	else
		error = git_buf_printf(out, "%-8" PRIuZ "%-8" PRIuZ "%s\n", insertions, deletions, path);

	return error;
}

int git_diff_file_stats__summary_to_buf(
	git_buf *out,
	const git_patch *patch)
{
	const git_diff_delta *delta = NULL;

	delta = git_patch_get_delta(patch);

	if (delta->old_file.mode != delta->new_file.mode) {
		if (delta->old_file.mode == 0) {
			git_buf_printf(out, " create mode %06o %s\n",
				delta->new_file.mode, delta->new_file.path);
		}
		else if (delta->new_file.mode == 0) {
			git_buf_printf(out, " delete mode %06o %s\n",
				delta->old_file.mode, delta->old_file.path);
		}
		else {
			git_buf_printf(out, " mode change %06o => %06o %s\n",
				delta->old_file.mode, delta->new_file.mode, delta->new_file.path);
		}
	}

	return 0;
}

int git_diff_stats__has_renames(
	const git_diff_stats *stats)
{
	git_patch *patch = NULL;
	size_t i;

	git_vector_foreach(&stats->patches, i, patch) {
		const git_diff_delta *delta = git_patch_get_delta(patch);

		if (strcmp(delta->old_file.path, delta->new_file.path) != 0) {
			return 1;
		}
	}

	return 0;
}

int git_diff_stats__add_file_stats(
	git_diff_stats *stats,
	git_patch *patch)
{
	const git_diff_delta *delta = NULL;
	int error = 0;

	if ((delta = git_patch_get_delta(patch)) == NULL)
		return -1;

	if ((error = git_vector_insert(&stats->patches, patch)) < 0)
		return error;

	return error;
}

int git_diff_get_stats(
	git_diff_stats **out,
	git_diff *diff)
{
	size_t i, deltas;
	size_t total_insertions = 0, total_deletions = 0;
	git_diff_stats *stats = NULL;
	int error = 0;

	assert(out && diff);

	stats = git__calloc(1, sizeof(git_diff_stats));
	GITERR_CHECK_ALLOC(stats);

	deltas = git_diff_num_deltas(diff);

	for (i = 0; i < deltas; ++i) {
		git_patch *patch = NULL;
		size_t add, remove;

		if ((error = git_patch_from_diff(&patch, diff, i)) < 0)
			goto on_error;

		if ((error = git_patch_line_stats(NULL, &add, &remove, patch)) < 0 ||
			(error = git_diff_stats__add_file_stats(stats, patch)) < 0) {
			git_patch_free(patch);
			goto on_error;
		}

		total_insertions += add;
		total_deletions += remove;
	}

	stats->files_changed = deltas;
	stats->insertions = total_insertions;
	stats->deletions = total_deletions;

	*out = stats;

	goto done;

on_error:
	git_diff_stats_free(stats);

done:
	return error;
}

size_t git_diff_stats_files_changed(
	const git_diff_stats *stats)
{
	assert(stats);

	return stats->files_changed;
}

size_t git_diff_stats_insertions(
	const git_diff_stats *stats)
{
	assert(stats);

	return stats->insertions;
}

size_t git_diff_stats_deletions(
	const git_diff_stats *stats)
{
	assert(stats);

	return stats->deletions;
}

int git_diff_stats_to_buf(
	git_buf *out,
	const git_diff_stats *stats,
	git_diff_stats_format_t format)
{
	git_patch *patch = NULL;
	size_t i;
	int has_renames = 0, error = 0;

	assert(out && stats);

	/* check if we have renames, it affects the padding */
	has_renames = git_diff_stats__has_renames(stats);

	git_vector_foreach(&stats->patches, i, patch) {
		if (format & GIT_DIFF_STATS_FULL) {
			size_t max_padding = diff_get_filename_padding(has_renames, stats);

			error = git_diff_file_stats__full_to_buf(out, max_padding, has_renames, patch);
		}
		else if (format & GIT_DIFF_STATS_NUMBER) {
			error = git_diff_file_stats__number_to_buf(out, patch);
		}

		if (error < 0)
			return error;
	}

	if (format & GIT_DIFF_STATS_FULL || format & GIT_DIFF_STATS_SHORT) {
		error = git_buf_printf(out, " %" PRIuZ " file%s changed, %" PRIuZ " insertions(+), %" PRIuZ " deletions(-)\n",
					stats->files_changed, stats->files_changed > 1 ? "s" : "",
					stats->insertions, stats->deletions);

		if (error < 0)
			return error;
	}

	if (format & GIT_DIFF_STATS_INCLUDE_SUMMARY) {
		git_vector_foreach(&stats->patches, i, patch) {
			if ((error = git_diff_file_stats__summary_to_buf(out, patch)) < 0)
				return error;
		}

		if (git_vector_length(&stats->patches) > 0)
			git_buf_putc(out, '\n');
	}

	return error;
}

void git_diff_stats_free(git_diff_stats *stats)
{
	size_t i;
	git_patch *patch;

	if (stats == NULL)
		return;

	git_vector_foreach(&stats->patches, i, patch)
		git_patch_free(patch);

	git_vector_free(&stats->patches);
	git__free(stats);
}

