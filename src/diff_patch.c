/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#include "common.h"
#include "diff.h"
#include "diff_file.h"
#include "diff_driver.h"
#include "diff_patch.h"
#include "diff_xdiff.h"

static void diff_output_init(git_diff_output*, const git_diff_options*,
	git_diff_file_cb, git_diff_hunk_cb, git_diff_data_cb, void*);

static void diff_output_to_patch(git_diff_output *, git_diff_patch *);

static void diff_patch_update_binary(git_diff_patch *patch)
{
	if ((patch->delta->flags & DIFF_FLAGS_KNOWN_BINARY) != 0)
		return;

	if ((patch->ofile.file.flags & GIT_DIFF_FLAG_BINARY) != 0 ||
		(patch->nfile.file.flags & GIT_DIFF_FLAG_BINARY) != 0)
		patch->delta->flags |= GIT_DIFF_FLAG_BINARY;

	else if ((patch->ofile.file.flags & DIFF_FLAGS_NOT_BINARY) != 0 &&
			 (patch->nfile.file.flags & DIFF_FLAGS_NOT_BINARY) != 0)
		patch->delta->flags |= GIT_DIFF_FLAG_NOT_BINARY;
}

static void diff_patch_init_common(git_diff_patch *patch)
{
	diff_patch_update_binary(patch);

	if ((patch->delta->flags & GIT_DIFF_FLAG_BINARY) != 0)
		patch->flags |= GIT_DIFF_PATCH_LOADED; /* set LOADED but not DIFFABLE */

	patch->flags |= GIT_DIFF_PATCH_INITIALIZED;

	if (patch->diff)
		git_diff_list_addref(patch->diff);
}

static int diff_patch_init_from_diff(
	git_diff_patch *patch, git_diff_list *diff, size_t delta_index)
{
	int error = 0;

	memset(patch, 0, sizeof(*patch));
	patch->diff  = diff;
	patch->delta = git_vector_get(&diff->deltas, delta_index);
	patch->delta_index = delta_index;

	if ((error = diff_file_content_init_from_diff(
			&patch->ofile, diff, delta_index, true)) < 0 ||
		(error = diff_file_content_init_from_diff(
			&patch->nfile, diff, delta_index, false)) < 0)
		return error;

	diff_patch_init_common(patch);

	return 0;
}

static int diff_patch_alloc_from_diff(
	git_diff_patch **out,
	git_diff_list *diff,
	size_t delta_index)
{
	int error;
	git_diff_patch *patch = git__calloc(1, sizeof(git_diff_patch));
	GITERR_CHECK_ALLOC(patch);

	if (!(error = diff_patch_init_from_diff(patch, diff, delta_index))) {
		patch->flags |= GIT_DIFF_PATCH_ALLOCATED;
		GIT_REFCOUNT_INC(patch);
	} else {
		git__free(patch);
		patch = NULL;
	}

	*out = patch;
	return error;
}

static int diff_patch_load(git_diff_patch *patch, git_diff_output *output)
{
	int error = 0;
	bool incomplete_data;

	if ((patch->flags & GIT_DIFF_PATCH_LOADED) != 0)
		return 0;

	/* if no hunk and data callbacks and user doesn't care if data looks
	 * binary, then there is no need to actually load the data
	 */
	if ((patch->ofile.opts_flags & GIT_DIFF_SKIP_BINARY_CHECK) != 0 &&
		output && !output->hunk_cb && !output->data_cb)
		return 0;

#define DIFF_FLAGS_KNOWN_DATA (GIT_DIFF_FLAG__NO_DATA|GIT_DIFF_FLAG_VALID_OID)

	incomplete_data =
		((patch->ofile.file.flags & DIFF_FLAGS_KNOWN_DATA) != 0 &&
		 (patch->nfile.file.flags & DIFF_FLAGS_KNOWN_DATA) != 0);

	/* always try to load workdir content first because filtering may
	 * need 2x data size and this minimizes peak memory footprint
	 */
	if (patch->ofile.src == GIT_ITERATOR_TYPE_WORKDIR) {
		if ((error = diff_file_content_load(&patch->ofile)) < 0 ||
			(patch->ofile.file.flags & GIT_DIFF_FLAG_BINARY) != 0)
			goto cleanup;
	}
	if (patch->nfile.src == GIT_ITERATOR_TYPE_WORKDIR) {
		if ((error = diff_file_content_load(&patch->nfile)) < 0 ||
			(patch->nfile.file.flags & GIT_DIFF_FLAG_BINARY) != 0)
			goto cleanup;
	}

	/* once workdir has been tried, load other data as needed */
	if (patch->ofile.src != GIT_ITERATOR_TYPE_WORKDIR) {
		if ((error = diff_file_content_load(&patch->ofile)) < 0 ||
			(patch->ofile.file.flags & GIT_DIFF_FLAG_BINARY) != 0)
			goto cleanup;
	}
	if (patch->nfile.src != GIT_ITERATOR_TYPE_WORKDIR) {
		if ((error = diff_file_content_load(&patch->nfile)) < 0 ||
			(patch->nfile.file.flags & GIT_DIFF_FLAG_BINARY) != 0)
			goto cleanup;
	}

	/* if we were previously missing an oid, reassess UNMODIFIED state */
	if (incomplete_data &&
		patch->ofile.file.mode == patch->nfile.file.mode &&
		git_oid_equal(&patch->ofile.file.oid, &patch->nfile.file.oid))
		patch->delta->status = GIT_DELTA_UNMODIFIED;

cleanup:
	diff_patch_update_binary(patch);

	if (!error) {
		/* patch is diffable only for non-binary, modified files where
		 * at least one side has data and the data actually changed
		 */
		if ((patch->delta->flags & GIT_DIFF_FLAG_BINARY) == 0 &&
			patch->delta->status != GIT_DELTA_UNMODIFIED &&
			(patch->ofile.map.len || patch->nfile.map.len) &&
			(patch->ofile.map.len != patch->nfile.map.len ||
			 !git_oid_equal(&patch->ofile.file.oid, &patch->nfile.file.oid)))
			patch->flags |= GIT_DIFF_PATCH_DIFFABLE;

		patch->flags |= GIT_DIFF_PATCH_LOADED;
	}

	return error;
}

static int diff_patch_file_callback(
	git_diff_patch *patch, git_diff_output *output)
{
	float progress;

	if (!output->file_cb)
		return 0;

	progress = patch->diff ?
		((float)patch->delta_index / patch->diff->deltas.length) : 1.0f;

	if (output->file_cb(patch->delta, progress, output->payload) != 0)
		output->error = GIT_EUSER;

	return output->error;
}

static int diff_patch_generate(git_diff_patch *patch, git_diff_output *output)
{
	int error = 0;

	if ((patch->flags & GIT_DIFF_PATCH_DIFFED) != 0)
		return 0;

	if ((patch->flags & GIT_DIFF_PATCH_LOADED) == 0 &&
		(error = diff_patch_load(patch, output)) < 0)
		return error;

	if ((patch->flags & GIT_DIFF_PATCH_DIFFABLE) == 0)
		return 0;

	if (output->diff_cb != NULL &&
		!(error = output->diff_cb(output, patch)))
		patch->flags |= GIT_DIFF_PATCH_DIFFED;

	return error;
}

static void diff_patch_free(git_diff_patch *patch)
{
	diff_file_content_clear(&patch->ofile);
	diff_file_content_clear(&patch->nfile);

	git_array_clear(patch->lines);
	git_array_clear(patch->hunks);

	git_diff_list_free(patch->diff); /* decrements refcount */
	patch->diff = NULL;

	git_pool_clear(&patch->flattened);

	if (patch->flags & GIT_DIFF_PATCH_ALLOCATED)
		git__free(patch);
}

static int diff_required(git_diff_list *diff, const char *action)
{
	if (diff)
		return 0;
	giterr_set(GITERR_INVALID, "Must provide valid diff to %s", action);
	return -1;
}

int git_diff_foreach(
	git_diff_list *diff,
	git_diff_file_cb file_cb,
	git_diff_hunk_cb hunk_cb,
	git_diff_data_cb data_cb,
	void *payload)
{
	int error = 0;
	git_xdiff_output xo;
	size_t idx;
	git_diff_patch patch;

	if (diff_required(diff, "git_diff_foreach") < 0)
		return -1;

	diff_output_init((git_diff_output *)&xo,
		&diff->opts, file_cb, hunk_cb, data_cb, payload);
	git_xdiff_init(&xo, &diff->opts);

	git_vector_foreach(&diff->deltas, idx, patch.delta) {
		/* check flags against patch status */
		if (git_diff_delta__should_skip(&diff->opts, patch.delta))
			continue;

		if (!(error = diff_patch_init_from_diff(&patch, diff, idx))) {

			error = diff_patch_file_callback(&patch, (git_diff_output *)&xo);

			if (!error)
				error = diff_patch_generate(&patch, (git_diff_output *)&xo);

			git_diff_patch_free(&patch);
		}

		if (error < 0)
			break;
	}

	if (error == GIT_EUSER)
		giterr_clear(); /* don't leave error message set invalidly */
	return error;
}

typedef struct {
	git_xdiff_output xo;
	git_diff_patch patch;
	git_diff_delta delta;
} diff_single_info;

static int diff_single_generate(diff_single_info *info)
{
	int error = 0;
	git_diff_patch *patch = &info->patch;
	bool has_old = ((patch->ofile.file.flags & GIT_DIFF_FLAG__NO_DATA) == 0);
	bool has_new = ((patch->nfile.file.flags & GIT_DIFF_FLAG__NO_DATA) == 0);

	info->delta.status = has_new ?
		(has_old ? GIT_DELTA_MODIFIED : GIT_DELTA_ADDED) :
		(has_old ? GIT_DELTA_DELETED : GIT_DELTA_UNTRACKED);

	if (git_oid_equal(&patch->nfile.file.oid, &patch->ofile.file.oid))
		info->delta.status = GIT_DELTA_UNMODIFIED;

	patch->delta = &info->delta;

	diff_patch_init_common(patch);

	error = diff_patch_file_callback(patch, (git_diff_output *)&info->xo);

	if (!error)
		error = diff_patch_generate(patch, (git_diff_output *)&info->xo);

	if (error == GIT_EUSER)
		giterr_clear(); /* don't leave error message set invalidly */

	return error;
}

int git_diff_blobs(
	const git_blob *old_blob,
	const git_blob *new_blob,
	const git_diff_options *opts,
	git_diff_file_cb file_cb,
	git_diff_hunk_cb hunk_cb,
	git_diff_data_cb data_cb,
	void *payload)
{
	int error = 0;
	diff_single_info info;
	git_repository *repo =
		new_blob ? git_object_owner((const git_object *)new_blob) :
		old_blob ? git_object_owner((const git_object *)old_blob) : NULL;

	GITERR_CHECK_VERSION(opts, GIT_DIFF_OPTIONS_VERSION, "git_diff_options");

	if (!repo) /* Hmm, given two NULL blobs, silently do no callbacks? */
		return 0;

	if (opts && (opts->flags & GIT_DIFF_REVERSE) != 0) {
		const git_blob *swap = old_blob;
		old_blob = new_blob;
		new_blob = swap;
	}

	memset(&info, 0, sizeof(info));

	diff_output_init((git_diff_output *)&info.xo,
		opts, file_cb, hunk_cb, data_cb, payload);
	git_xdiff_init(&info.xo, opts);

	if (!(error = diff_file_content_init_from_blob(
			&info.patch.ofile, repo, opts, old_blob)) &&
		!(error = diff_file_content_init_from_blob(
			&info.patch.nfile, repo, opts, new_blob)))
		error = diff_single_generate(&info);

	git_diff_patch_free(&info.patch);

	return error;
}

int git_diff_blob_to_buffer(
	const git_blob *old_blob,
	const char *buf,
	size_t buflen,
	const git_diff_options *opts,
	git_diff_file_cb file_cb,
	git_diff_hunk_cb hunk_cb,
	git_diff_data_cb data_cb,
	void *payload)
{
	int error = 0;
	diff_single_info info;
	git_repository *repo =
		old_blob ? git_object_owner((const git_object *)old_blob) : NULL;

	GITERR_CHECK_VERSION(opts, GIT_DIFF_OPTIONS_VERSION, "git_diff_options");

	if (!repo && !buf) /* Hmm, given NULLs, silently do no callbacks? */
		return 0;

	memset(&info, 0, sizeof(info));

	diff_output_init((git_diff_output *)&info.xo,
		opts, file_cb, hunk_cb, data_cb, payload);
	git_xdiff_init(&info.xo, opts);

	if (opts && (opts->flags & GIT_DIFF_REVERSE) != 0) {
		if (!(error = diff_file_content_init_from_raw(
				&info.patch.ofile, repo, opts, buf, buflen)))
			error = diff_file_content_init_from_blob(
				&info.patch.nfile, repo, opts, old_blob);
	} else {
		if (!(error = diff_file_content_init_from_blob(
				&info.patch.ofile, repo, opts, old_blob)))
			error = diff_file_content_init_from_raw(
				&info.patch.nfile, repo, opts, buf, buflen);
	}

	error = diff_single_generate(&info);

	git_diff_patch_free(&info.patch);

	return error;
}

int git_diff_get_patch(
	git_diff_patch **patch_ptr,
	const git_diff_delta **delta_ptr,
	git_diff_list *diff,
	size_t idx)
{
	int error = 0;
	git_xdiff_output xo;
	git_diff_delta *delta = NULL;
	git_diff_patch *patch = NULL;

	if (patch_ptr) *patch_ptr = NULL;
	if (delta_ptr) *delta_ptr = NULL;

	if (diff_required(diff, "git_diff_get_patch") < 0)
		return -1;

	delta = git_vector_get(&diff->deltas, idx);
	if (!delta) {
		giterr_set(GITERR_INVALID, "Index out of range for delta in diff");
		return GIT_ENOTFOUND;
	}

	if (delta_ptr)
		*delta_ptr = delta;

	if (git_diff_delta__should_skip(&diff->opts, delta))
		return 0;

	/* don't load the patch data unless we need it for binary check */
	if (!patch_ptr &&
		((delta->flags & DIFF_FLAGS_KNOWN_BINARY) != 0 ||
		 (diff->opts.flags & GIT_DIFF_SKIP_BINARY_CHECK) != 0))
		return 0;

	if ((error = diff_patch_alloc_from_diff(&patch, diff, idx)) < 0)
		return error;

	diff_output_to_patch((git_diff_output *)&xo, patch);
	git_xdiff_init(&xo, &diff->opts);

	error = diff_patch_file_callback(patch, (git_diff_output *)&xo);

	if (!error)
		error = diff_patch_generate(patch, (git_diff_output *)&xo);

	if (!error) {
		/* if cumulative diff size is < 0.5 total size, flatten the patch */
		/* unload the file content */
	}

	if (error || !patch_ptr)
		git_diff_patch_free(patch);
	else
		*patch_ptr = patch;

	if (error == GIT_EUSER)
		giterr_clear(); /* don't leave error message set invalidly */
	return error;
}

void git_diff_patch_free(git_diff_patch *patch)
{
	if (patch)
		GIT_REFCOUNT_DEC(patch, diff_patch_free);
}

const git_diff_delta *git_diff_patch_delta(git_diff_patch *patch)
{
	assert(patch);
	return patch->delta;
}

size_t git_diff_patch_num_hunks(git_diff_patch *patch)
{
	assert(patch);
	return git_array_size(patch->hunks);
}

int git_diff_patch_line_stats(
	size_t *total_ctxt,
	size_t *total_adds,
	size_t *total_dels,
	const git_diff_patch *patch)
{
	size_t totals[3], idx;

	memset(totals, 0, sizeof(totals));

	for (idx = 0; idx < git_array_size(patch->lines); ++idx) {
		diff_patch_line *line = git_array_get(patch->lines, idx);
		if (!line)
			continue;

		switch (line->origin) {
		case GIT_DIFF_LINE_CONTEXT:  totals[0]++; break;
		case GIT_DIFF_LINE_ADDITION: totals[1]++; break;
		case GIT_DIFF_LINE_DELETION: totals[2]++; break;
		default:
			/* diff --stat and --numstat don't count EOFNL marks because
			 * they will always be paired with a ADDITION or DELETION line.
			 */
			break;
		}
	}

	if (total_ctxt)
		*total_ctxt = totals[0];
	if (total_adds)
		*total_adds = totals[1];
	if (total_dels)
		*total_dels = totals[2];

	return 0;
}

static int diff_error_outofrange(const char *thing)
{
	giterr_set(GITERR_INVALID, "Diff patch %s index out of range", thing);
	return GIT_ENOTFOUND;
}

int git_diff_patch_get_hunk(
	const git_diff_range **range,
	const char **header,
	size_t *header_len,
	size_t *lines_in_hunk,
	git_diff_patch *patch,
	size_t hunk_idx)
{
	diff_patch_hunk *hunk;
	assert(patch);

	hunk = git_array_get(patch->hunks, hunk_idx);

	if (!hunk) {
		if (range) *range = NULL;
		if (header) *header = NULL;
		if (header_len) *header_len = 0;
		if (lines_in_hunk) *lines_in_hunk = 0;
		return diff_error_outofrange("hunk");
	}

	if (range) *range = &hunk->range;
	if (header) *header = hunk->header;
	if (header_len) *header_len = hunk->header_len;
	if (lines_in_hunk) *lines_in_hunk = hunk->line_count;
	return 0;
}

int git_diff_patch_num_lines_in_hunk(git_diff_patch *patch, size_t hunk_idx)
{
	diff_patch_hunk *hunk;
	assert(patch);

	if (!(hunk = git_array_get(patch->hunks, hunk_idx)))
		return diff_error_outofrange("hunk");
	return (int)hunk->line_count;
}

int git_diff_patch_get_line_in_hunk(
	char *line_origin,
	const char **content,
	size_t *content_len,
	int *old_lineno,
	int *new_lineno,
	git_diff_patch *patch,
	size_t hunk_idx,
	size_t line_of_hunk)
{
	diff_patch_hunk *hunk;
	diff_patch_line *line;
	const char *thing;

	assert(patch);

	if (!(hunk = git_array_get(patch->hunks, hunk_idx))) {
		thing = "hunk";
		goto notfound;
	}

	if (line_of_hunk >= hunk->line_count ||
		!(line = git_array_get(
			patch->lines, hunk->line_start + line_of_hunk))) {
		thing = "line";
		goto notfound;
	}

	if (line_origin) *line_origin = line->origin;
	if (content) *content = line->ptr;
	if (content_len) *content_len = line->len;
	if (old_lineno) *old_lineno = (int)line->oldno;
	if (new_lineno) *new_lineno = (int)line->newno;

	return 0;

notfound:
	if (line_origin) *line_origin = GIT_DIFF_LINE_CONTEXT;
	if (content) *content = NULL;
	if (content_len) *content_len = 0;
	if (old_lineno) *old_lineno = -1;
	if (new_lineno) *new_lineno = -1;

	return diff_error_outofrange(thing);
}


static int diff_patch_file_cb(
	const git_diff_delta *delta,
	float progress,
	void *payload)
{
	GIT_UNUSED(delta);
	GIT_UNUSED(progress);
	GIT_UNUSED(payload);
	return 0;
}

static int diff_patch_hunk_cb(
	const git_diff_delta *delta,
	const git_diff_range *range,
	const char *header,
	size_t header_len,
	void *payload)
{
	git_diff_patch *patch = payload;
	diff_patch_hunk *hunk;

	GIT_UNUSED(delta);

	git_array_alloc(patch->hunks, hunk);
	GITERR_CHECK_ALLOC(hunk);

	memcpy(&hunk->range, range, sizeof(hunk->range));

	assert(header_len + 1 < sizeof(hunk->header));
	memcpy(&hunk->header, header, header_len);
	hunk->header[header_len] = '\0';
	hunk->header_len = header_len;

	hunk->line_start = git_array_size(patch->lines);
	hunk->line_count = 0;

	patch->oldno = range->old_start;
	patch->newno = range->new_start;

	return 0;
}

static int diff_patch_line_cb(
	const git_diff_delta *delta,
	const git_diff_range *range,
	char line_origin,
	const char *content,
	size_t content_len,
	void *payload)
{
	git_diff_patch *patch = payload;
	diff_patch_hunk *hunk;
	diff_patch_line *line;

	GIT_UNUSED(delta);
	GIT_UNUSED(range);

	hunk = git_array_last(patch->hunks);
	GITERR_CHECK_ALLOC(hunk);

	git_array_alloc(patch->lines, line);
	GITERR_CHECK_ALLOC(line);

	line->ptr = content;
	line->len = content_len;
	line->origin = line_origin;

	patch->content_size += content_len;

	/* do some bookkeeping so we can provide old/new line numbers */

	for (line->lines = 0; content_len > 0; --content_len) {
		if (*content++ == '\n')
			++line->lines;
	}

	switch (line_origin) {
	case GIT_DIFF_LINE_ADDITION:
	case GIT_DIFF_LINE_DEL_EOFNL:
		line->oldno = -1;
		line->newno = patch->newno;
		patch->newno += line->lines;
		break;
	case GIT_DIFF_LINE_DELETION:
	case GIT_DIFF_LINE_ADD_EOFNL:
		line->oldno = patch->oldno;
		line->newno = -1;
		patch->oldno += line->lines;
		break;
	default:
		line->oldno = patch->oldno;
		line->newno = patch->newno;
		patch->oldno += line->lines;
		patch->newno += line->lines;
		break;
	}

	hunk->line_count++;

	return 0;
}

static void diff_output_init(
	git_diff_output *out,
	const git_diff_options *opts,
	git_diff_file_cb file_cb,
	git_diff_hunk_cb hunk_cb,
	git_diff_data_cb data_cb,
	void *payload)
{
	GIT_UNUSED(opts);

	memset(out, 0, sizeof(*out));

	out->file_cb = file_cb;
	out->hunk_cb = hunk_cb;
	out->data_cb = data_cb;
	out->payload = payload;
}

static void diff_output_to_patch(git_diff_output *out, git_diff_patch *patch)
{
	diff_output_init(
		out, NULL,
		diff_patch_file_cb, diff_patch_hunk_cb, diff_patch_line_cb, patch);
}
