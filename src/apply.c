/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include <assert.h>

#include "git2/patch.h"
#include "git2/filter.h"
#include "array.h"
#include "diff_patch.h"
#include "fileops.h"
#include "apply.h"

#define apply_err(...) \
	( giterr_set(GITERR_PATCH, __VA_ARGS__), -1 )

typedef struct {
	/* The lines that we allocate ourself are allocated out of the pool.
	 * (Lines may have been allocated out of the diff.)
	 */
	git_pool pool;
	git_vector lines;
} patch_image;

static void patch_line_init(
	git_diff_line *out,
	const char *in,
	size_t in_len,
	size_t in_offset)
{
	out->content = in;
	out->content_len = in_len;
	out->content_offset = in_offset;
}

static unsigned int patch_image_init(patch_image *out)
{
	memset(out, 0x0, sizeof(patch_image));
	return 0;
}

static int patch_image_init_fromstr(
	patch_image *out, const char *in, size_t in_len)
{
	git_diff_line *line;
	const char *start, *end;

	memset(out, 0x0, sizeof(patch_image));

	git_pool_init(&out->pool, sizeof(git_diff_line));

	for (start = in; start < in + in_len; start = end) {
		end = memchr(start, '\n', in_len);

		if (end < in + in_len)
			end++;

		line = git_pool_mallocz(&out->pool, 1);
		GITERR_CHECK_ALLOC(line);

		if (git_vector_insert(&out->lines, line) < 0)
			return -1;

		patch_line_init(line, start, (end - start), (start - in));
	}

	return 0;
}

static void patch_image_free(patch_image *image)
{
	if (image == NULL)
		return;

	git_pool_clear(&image->pool);
	git_vector_free(&image->lines);
}

static bool match_hunk(
	patch_image *image,
	patch_image *preimage,
	size_t linenum)
{
	bool match = 0;
	size_t i;

	/* Ensure this hunk is within the image boundaries. */
	if (git_vector_length(&preimage->lines) + linenum >
		git_vector_length(&image->lines))
		return 0;

	match = 1;

	/* Check exact match. */
	for (i = 0; i < git_vector_length(&preimage->lines); i++) {
		git_diff_line *preimage_line = git_vector_get(&preimage->lines, i);
		git_diff_line *image_line = git_vector_get(&image->lines, linenum + i);

		if (preimage_line->content_len != preimage_line->content_len ||
			memcmp(preimage_line->content, image_line->content, image_line->content_len) != 0) {
			match = 0;
			break;
		}
	}

	return match;
}

static bool find_hunk_linenum(
	size_t *out,
	patch_image *image,
	patch_image *preimage,
	size_t linenum)
{
	size_t max = git_vector_length(&image->lines);
	bool match;

	if (linenum > max)
		linenum = max;

	match = match_hunk(image, preimage, linenum);

	*out = linenum;
	return match;
}

static int update_hunk(
	patch_image *image,
	unsigned int linenum,
	patch_image *preimage,
	patch_image *postimage)
{
	size_t postlen = git_vector_length(&postimage->lines);
	size_t prelen = git_vector_length(&preimage->lines);
	size_t i;
	int error = 0;

	if (postlen > prelen)
		error = git_vector_grow_at(
			&image->lines, linenum, (postlen - prelen));
	else if (prelen > postlen)
		error = git_vector_shrink_at(
			&image->lines, linenum, (prelen - postlen));

	if (error) {
		giterr_set_oom();
		return -1;
	}

	for (i = 0; i < git_vector_length(&postimage->lines); i++) {
		image->lines.contents[linenum + i] =
			git_vector_get(&postimage->lines, i);
	}

	return 0;
}

static int apply_hunk(
	patch_image *image,
	git_patch *patch,
	diff_patch_hunk *hunk)
{
	patch_image preimage, postimage;
	size_t line_num, i;
	int error = 0;

	if ((error = patch_image_init(&preimage)) < 0 ||
		(error = patch_image_init(&postimage)) < 0)
		goto done;

	for (i = 0; i < hunk->line_count; i++) {
		size_t linenum = hunk->line_start + i;
		git_diff_line *line = git_array_get(patch->lines, linenum);

		if (!line) {
			error = apply_err("Preimage does not contain line %d", linenum);
			goto done;
		}

		if (line->origin == GIT_DIFF_LINE_CONTEXT ||
			line->origin == GIT_DIFF_LINE_DELETION) {
			if ((error = git_vector_insert(&preimage.lines, line)) < 0)
				goto done;
		}

		if (line->origin == GIT_DIFF_LINE_CONTEXT ||
			line->origin == GIT_DIFF_LINE_ADDITION) {
			if ((error = git_vector_insert(&postimage.lines, line)) < 0)
				goto done;
		}
	}

	line_num = hunk->hunk.new_start ? hunk->hunk.new_start - 1 : 0;

	if (!find_hunk_linenum(&line_num, image, &preimage, line_num)) {
		error = apply_err("Hunk at line %d did not apply",
			hunk->hunk.new_start);
		goto done;
	}

	error = update_hunk(image, line_num, &preimage, &postimage);

done:
	patch_image_free(&preimage);
	patch_image_free(&postimage);

	return error;
}

static int apply_hunks(
	git_buf *out,
	const char *source,
	size_t source_len,
	git_patch *patch)
{
	diff_patch_hunk *hunk;
	git_diff_line *line;
	patch_image image;
	size_t i;
	int error = 0;

	if ((error = patch_image_init_fromstr(&image, source, source_len)) < 0)
		goto done;

	git_array_foreach(patch->hunks, i, hunk) {
		if ((error = apply_hunk(&image, patch, hunk)) < 0)
			goto done;
	}

	git_vector_foreach(&image.lines, i, line)
		git_buf_put(out, line->content, line->content_len);

done:
	patch_image_free(&image);

	return error;
}

int git_apply__patch(
	git_buf *contents_out,
	char **filename_out,
	unsigned int *mode_out,
	const char *source,
	size_t source_len,
	git_patch *patch)
{
	char *filename = NULL;
	unsigned int mode = 0;
	int error = 0;

	assert(contents_out && filename_out && mode_out && (source || !source_len) && patch);

	*filename_out = NULL;
	*mode_out = 0;

	if (patch->delta->status != GIT_DELTA_DELETED) {
		filename = git__strdup(patch->nfile.file->path);
		mode = patch->nfile.file->mode ?
			patch->nfile.file->mode : GIT_FILEMODE_BLOB;
	}

	/* If the patch is empty, simply keep the source unchanged */
	if (patch->hunks.size == 0)
		git_buf_put(contents_out, source, source_len);
	else if ((error = apply_hunks(contents_out, source, source_len, patch)) < 0)
		goto done;

	if (patch->delta->status == GIT_DELTA_DELETED &&
		git_buf_len(contents_out) > 0) {
		error = apply_err("removal patch leaves file contents");
		goto done;
	}

	*filename_out = filename;
	*mode_out = mode;

done:
	if (error < 0)
		git__free(filename);

	return error;
}
