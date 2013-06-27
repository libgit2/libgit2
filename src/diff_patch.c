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
#include "fileops.h"

/* cached information about a single span in a diff */
typedef struct diff_patch_line diff_patch_line;
struct diff_patch_line {
	const char *ptr;
	size_t len;
	size_t lines, oldno, newno;
	char origin;
};

/* cached information about a hunk in a diff */
typedef struct diff_patch_hunk diff_patch_hunk;
struct diff_patch_hunk {
	git_diff_range range;
	char   header[128];
	size_t header_len;
	size_t line_start;
	size_t line_count;
};

struct git_diff_patch {
	git_refcount rc;
	git_diff_list *diff; /* for refcount purposes, maybe NULL for blob diffs */
	git_diff_delta *delta;
	size_t delta_index;
	git_diff_file_content ofile;
	git_diff_file_content nfile;
	uint32_t flags;
	git_array_t(diff_patch_hunk) hunks;
	git_array_t(diff_patch_line) lines;
	size_t oldno, newno;
	size_t content_size;
	git_pool flattened;
};

enum {
	GIT_DIFF_PATCH_ALLOCATED   = (1 << 0),
	GIT_DIFF_PATCH_INITIALIZED = (1 << 1),
	GIT_DIFF_PATCH_LOADED      = (1 << 2),
	GIT_DIFF_PATCH_DIFFABLE    = (1 << 3),
	GIT_DIFF_PATCH_DIFFED      = (1 << 4),
	GIT_DIFF_PATCH_FLATTENED   = (1 << 5),
};

static void diff_output_init(git_diff_output*, const git_diff_options*,
	git_diff_file_cb, git_diff_hunk_cb, git_diff_data_cb, void*);

static void diff_output_to_patch(git_diff_output *, git_diff_patch *);

static void diff_patch_update_binary(git_diff_patch *patch)
{
	if ((patch->delta->flags & DIFF_FLAGS_KNOWN_BINARY) != 0)
		return;

	if ((patch->ofile.file->flags & GIT_DIFF_FLAG_BINARY) != 0 ||
		(patch->nfile.file->flags & GIT_DIFF_FLAG_BINARY) != 0)
		patch->delta->flags |= GIT_DIFF_FLAG_BINARY;

	else if ((patch->ofile.file->flags & DIFF_FLAGS_NOT_BINARY) != 0 &&
			 (patch->nfile.file->flags & DIFF_FLAGS_NOT_BINARY) != 0)
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

	if ((error = git_diff_file_content__init_from_diff(
			&patch->ofile, diff, delta_index, true)) < 0 ||
		(error = git_diff_file_content__init_from_diff(
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

	incomplete_data =
		(((patch->ofile.flags & GIT_DIFF_FLAG__NO_DATA) != 0 ||
		  (patch->ofile.file->flags & GIT_DIFF_FLAG_VALID_OID) != 0) &&
		 ((patch->nfile.flags & GIT_DIFF_FLAG__NO_DATA) != 0 ||
		  (patch->nfile.file->flags & GIT_DIFF_FLAG_VALID_OID) != 0));

	/* always try to load workdir content first because filtering may
	 * need 2x data size and this minimizes peak memory footprint
	 */
	if (patch->ofile.src == GIT_ITERATOR_TYPE_WORKDIR) {
		if ((error = git_diff_file_content__load(&patch->ofile)) < 0 ||
			(patch->ofile.file->flags & GIT_DIFF_FLAG_BINARY) != 0)
			goto cleanup;
	}
	if (patch->nfile.src == GIT_ITERATOR_TYPE_WORKDIR) {
		if ((error = git_diff_file_content__load(&patch->nfile)) < 0 ||
			(patch->nfile.file->flags & GIT_DIFF_FLAG_BINARY) != 0)
			goto cleanup;
	}

	/* once workdir has been tried, load other data as needed */
	if (patch->ofile.src != GIT_ITERATOR_TYPE_WORKDIR) {
		if ((error = git_diff_file_content__load(&patch->ofile)) < 0 ||
			(patch->ofile.file->flags & GIT_DIFF_FLAG_BINARY) != 0)
			goto cleanup;
	}
	if (patch->nfile.src != GIT_ITERATOR_TYPE_WORKDIR) {
		if ((error = git_diff_file_content__load(&patch->nfile)) < 0 ||
			(patch->nfile.file->flags & GIT_DIFF_FLAG_BINARY) != 0)
			goto cleanup;
	}

	/* if previously missing an oid, and now that we have it the two sides
	 * are the same (and not submodules), update MODIFIED -> UNMODIFIED
	 */
	if (incomplete_data &&
		patch->ofile.file->mode == patch->nfile.file->mode &&
		patch->ofile.file->mode != GIT_FILEMODE_COMMIT &&
		git_oid_equal(&patch->ofile.file->oid, &patch->nfile.file->oid) &&
		patch->delta->status == GIT_DELTA_MODIFIED) /* not RENAMED/COPIED! */
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
			 !git_oid_equal(&patch->ofile.file->oid, &patch->nfile.file->oid)))
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
	git_diff_file_content__clear(&patch->ofile);
	git_diff_file_content__clear(&patch->nfile);

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
	git_diff_patch patch;
	git_diff_delta delta;
	char paths[GIT_FLEX_ARRAY];
} diff_patch_with_delta;

static int diff_single_generate(diff_patch_with_delta *pd, git_xdiff_output *xo)
{
	int error = 0;
	git_diff_patch *patch = &pd->patch;
	bool has_old = ((patch->ofile.flags & GIT_DIFF_FLAG__NO_DATA) == 0);
	bool has_new = ((patch->nfile.flags & GIT_DIFF_FLAG__NO_DATA) == 0);

	pd->delta.status = has_new ?
		(has_old ? GIT_DELTA_MODIFIED : GIT_DELTA_ADDED) :
		(has_old ? GIT_DELTA_DELETED : GIT_DELTA_UNTRACKED);

	if (git_oid_equal(&patch->nfile.file->oid, &patch->ofile.file->oid))
		pd->delta.status = GIT_DELTA_UNMODIFIED;

	patch->delta = &pd->delta;

	diff_patch_init_common(patch);

	if (pd->delta.status == GIT_DELTA_UNMODIFIED &&
		!(patch->ofile.opts_flags & GIT_DIFF_INCLUDE_UNMODIFIED))
		return error;

	error = diff_patch_file_callback(patch, (git_diff_output *)xo);

	if (!error)
		error = diff_patch_generate(patch, (git_diff_output *)xo);

	if (error == GIT_EUSER)
		giterr_clear(); /* don't leave error message set invalidly */

	return error;
}

static int diff_patch_from_blobs(
	diff_patch_with_delta *pd,
	git_xdiff_output *xo,
	const git_blob *old_blob,
	const char *old_path,
	const git_blob *new_blob,
	const char *new_path,
	const git_diff_options *opts)
{
	int error = 0;
	git_repository *repo =
		new_blob ? git_object_owner((const git_object *)new_blob) :
		old_blob ? git_object_owner((const git_object *)old_blob) : NULL;

	GITERR_CHECK_VERSION(opts, GIT_DIFF_OPTIONS_VERSION, "git_diff_options");

	if (opts && (opts->flags & GIT_DIFF_REVERSE) != 0) {
		const git_blob *tmp_blob;
		const char *tmp_path;
		tmp_blob = old_blob; old_blob = new_blob; new_blob = tmp_blob;
		tmp_path = old_path; old_path = new_path; new_path = tmp_path;
	}

	pd->patch.delta = &pd->delta;

	pd->delta.old_file.path = old_path;
	pd->delta.new_file.path = new_path;

	if ((error = git_diff_file_content__init_from_blob(
			&pd->patch.ofile, repo, opts, old_blob, &pd->delta.old_file)) < 0 ||
		(error = git_diff_file_content__init_from_blob(
			&pd->patch.nfile, repo, opts, new_blob, &pd->delta.new_file)) < 0)
		return error;

	return diff_single_generate(pd, xo);
}

static int diff_patch_with_delta_alloc(
	diff_patch_with_delta **out,
	const char **old_path,
	const char **new_path)
{
	diff_patch_with_delta *pd;
	size_t old_len = *old_path ? strlen(*old_path) : 0;
	size_t new_len = *new_path ? strlen(*new_path) : 0;

	*out = pd = git__calloc(1, sizeof(*pd) + old_len + new_len + 2);
	GITERR_CHECK_ALLOC(pd);

	pd->patch.flags = GIT_DIFF_PATCH_ALLOCATED;

	if (*old_path) {
		memcpy(&pd->paths[0], *old_path, old_len);
		*old_path = &pd->paths[0];
	} else if (*new_path)
		*old_path = &pd->paths[old_len + 1];

	if (*new_path) {
		memcpy(&pd->paths[old_len + 1], *new_path, new_len);
		*new_path = &pd->paths[old_len + 1];
	} else if (*old_path)
		*new_path = &pd->paths[0];

	return 0;
}

int git_diff_blobs(
	const git_blob *old_blob,
	const char *old_path,
	const git_blob *new_blob,
	const char *new_path,
	const git_diff_options *opts,
	git_diff_file_cb file_cb,
	git_diff_hunk_cb hunk_cb,
	git_diff_data_cb data_cb,
	void *payload)
{
	int error = 0;
	diff_patch_with_delta pd;
	git_xdiff_output xo;

	memset(&pd, 0, sizeof(pd));
	memset(&xo, 0, sizeof(xo));

	diff_output_init(
		(git_diff_output *)&xo, opts, file_cb, hunk_cb, data_cb, payload);
	git_xdiff_init(&xo, opts);

	if (!old_path && new_path)
		old_path = new_path;
	else if (!new_path && old_path)
		new_path = old_path;

	error = diff_patch_from_blobs(
		&pd, &xo, old_blob, old_path, new_blob, new_path, opts);

	git_diff_patch_free((git_diff_patch *)&pd);

	return error;
}

int git_diff_patch_from_blobs(
	git_diff_patch **out,
	const git_blob *old_blob,
	const char *old_path,
	const git_blob *new_blob,
	const char *new_path,
	const git_diff_options *opts)
{
	int error = 0;
	diff_patch_with_delta *pd;
	git_xdiff_output xo;

	assert(out);
	*out = NULL;

	if (diff_patch_with_delta_alloc(&pd, &old_path, &new_path) < 0)
		return -1;

	memset(&xo, 0, sizeof(xo));

	diff_output_to_patch((git_diff_output *)&xo, &pd->patch);
	git_xdiff_init(&xo, opts);

	error = diff_patch_from_blobs(
		pd, &xo, old_blob, old_path, new_blob, new_path, opts);

	if (!error)
		*out = (git_diff_patch *)pd;
	else
		git_diff_patch_free((git_diff_patch *)pd);

	return error;
}

static int diff_patch_from_blob_and_buffer(
	diff_patch_with_delta *pd,
	git_xdiff_output *xo,
	const git_blob *old_blob,
	const char *old_path,
	const char *buf,
	size_t buflen,
	const char *buf_path,
	const git_diff_options *opts)
{
	int error = 0;
	git_repository *repo =
		old_blob ? git_object_owner((const git_object *)old_blob) : NULL;

	GITERR_CHECK_VERSION(opts, GIT_DIFF_OPTIONS_VERSION, "git_diff_options");

	pd->patch.delta = &pd->delta;

	if (opts && (opts->flags & GIT_DIFF_REVERSE) != 0) {
		pd->delta.old_file.path = buf_path;
		pd->delta.new_file.path = old_path;

		if (!(error = git_diff_file_content__init_from_raw(
				&pd->patch.ofile, repo, opts, buf, buflen, &pd->delta.old_file)))
			error = git_diff_file_content__init_from_blob(
				&pd->patch.nfile, repo, opts, old_blob, &pd->delta.new_file);
	} else {
		pd->delta.old_file.path = old_path;
		pd->delta.new_file.path = buf_path;

		if (!(error = git_diff_file_content__init_from_blob(
				&pd->patch.ofile, repo, opts, old_blob, &pd->delta.old_file)))
			error = git_diff_file_content__init_from_raw(
				&pd->patch.nfile, repo, opts, buf, buflen, &pd->delta.new_file);
	}

	if (error < 0)
		return error;

	return diff_single_generate(pd, xo);
}

int git_diff_blob_to_buffer(
	const git_blob *old_blob,
	const char *old_path,
	const char *buf,
	size_t buflen,
	const char *buf_path,
	const git_diff_options *opts,
	git_diff_file_cb file_cb,
	git_diff_hunk_cb hunk_cb,
	git_diff_data_cb data_cb,
	void *payload)
{
	int error = 0;
	diff_patch_with_delta pd;
	git_xdiff_output xo;

	memset(&pd, 0, sizeof(pd));
	memset(&xo, 0, sizeof(xo));

	diff_output_init(
		(git_diff_output *)&xo, opts, file_cb, hunk_cb, data_cb, payload);
	git_xdiff_init(&xo, opts);

	if (!old_path && buf_path)
		old_path = buf_path;
	else if (!buf_path && old_path)
		buf_path = old_path;

	error = diff_patch_from_blob_and_buffer(
		&pd, &xo, old_blob, old_path, buf, buflen, buf_path, opts);

	git_diff_patch_free((git_diff_patch *)&pd);

	return error;
}

int git_diff_patch_from_blob_and_buffer(
	git_diff_patch **out,
	const git_blob *old_blob,
	const char *old_path,
	const char *buf,
	size_t buflen,
	const char *buf_path,
	const git_diff_options *opts)
{
	int error = 0;
	diff_patch_with_delta *pd;
	git_xdiff_output xo;

	assert(out);
	*out = NULL;

	if (diff_patch_with_delta_alloc(&pd, &old_path, &buf_path) < 0)
		return -1;

	memset(&xo, 0, sizeof(xo));

	diff_output_to_patch((git_diff_output *)&xo, &pd->patch);
	git_xdiff_init(&xo, opts);

	error = diff_patch_from_blob_and_buffer(
		pd, &xo, old_blob, old_path, buf, buflen, buf_path, opts);

	if (!error)
		*out = (git_diff_patch *)pd;
	else
		git_diff_patch_free((git_diff_patch *)pd);

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

git_diff_list *git_diff_patch__diff(git_diff_patch *patch)
{
	return patch->diff;
}

git_diff_driver *git_diff_patch__driver(git_diff_patch *patch)
{
	/* ofile driver is representative for whole patch */
	return patch->ofile.driver;
}

void git_diff_patch__old_data(
	char **ptr, size_t *len, git_diff_patch *patch)
{
	*ptr = patch->ofile.map.data;
	*len = patch->ofile.map.len;
}

void git_diff_patch__new_data(
	char **ptr, size_t *len, git_diff_patch *patch)
{
	*ptr = patch->nfile.map.data;
	*len = patch->nfile.map.len;
}

int git_diff_patch__invoke_callbacks(
	git_diff_patch *patch,
	git_diff_file_cb file_cb,
	git_diff_hunk_cb hunk_cb,
	git_diff_data_cb line_cb,
	void *payload)
{
	int error = 0;
	uint32_t i, j;

	if (file_cb)
		error = file_cb(patch->delta, 0, payload);

	if (!hunk_cb && !line_cb)
		return error;

	for (i = 0; !error && i < git_array_size(patch->hunks); ++i) {
		diff_patch_hunk *h = git_array_get(patch->hunks, i);

		error = hunk_cb(
			patch->delta, &h->range, h->header, h->header_len, payload);

		if (!line_cb)
			continue;

		for (j = 0; !error && j < h->line_count; ++j) {
			diff_patch_line *l =
				git_array_get(patch->lines, h->line_start + j);

			error = line_cb(
				patch->delta, &h->range, l->origin, l->ptr, l->len, payload);
		}
	}

	return error;
}


static int diff_patch_file_cb(
	const git_diff_delta *delta,
	float progress,
	void *payload)
{
	GIT_UNUSED(delta); GIT_UNUSED(progress); GIT_UNUSED(payload);
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

	hunk = git_array_alloc(patch->hunks);
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

	line = git_array_alloc(patch->lines);
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
