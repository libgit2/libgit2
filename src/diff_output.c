/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#include "common.h"
#include "git2/attr.h"
#include "git2/oid.h"
#include "git2/submodule.h"
#include "diff_output.h"
#include <ctype.h>
#include "fileops.h"
#include "filter.h"

static int read_next_int(const char **str, int *value)
{
	const char *scan = *str;
	int v = 0, digits = 0;
	/* find next digit */
	for (scan = *str; *scan && !isdigit(*scan); scan++);
	/* parse next number */
	for (; isdigit(*scan); scan++, digits++)
		v = (v * 10) + (*scan - '0');
	*str = scan;
	*value = v;
	return (digits > 0) ? 0 : -1;
}

static int parse_hunk_header(git_diff_range *range, const char *header)
{
	/* expect something of the form "@@ -%d[,%d] +%d[,%d] @@" */
	if (*header != '@')
		return -1;
	if (read_next_int(&header, &range->old_start) < 0)
		return -1;
	if (*header == ',') {
		if (read_next_int(&header, &range->old_lines) < 0)
			return -1;
	} else
		range->old_lines = 1;
	if (read_next_int(&header, &range->new_start) < 0)
		return -1;
	if (*header == ',') {
		if (read_next_int(&header, &range->new_lines) < 0)
			return -1;
	} else
		range->new_lines = 1;
	if (range->old_start < 0 || range->new_start < 0)
		return -1;

	return 0;
}

#define KNOWN_BINARY_FLAGS (GIT_DIFF_FLAG_BINARY|GIT_DIFF_FLAG_NOT_BINARY)
#define NOT_BINARY_FLAGS   (GIT_DIFF_FLAG_NOT_BINARY|GIT_DIFF_FLAG__NO_DATA)

static int update_file_is_binary_by_attr(
	git_repository *repo, git_diff_file *file)
{
	const char *value;

	/* because of blob diffs, cannot assume path is set */
	if (!file->path || !strlen(file->path))
		return 0;

	if (git_attr_get(&value, repo, 0, file->path, "diff") < 0)
		return -1;

	if (GIT_ATTR_FALSE(value))
		file->flags |= GIT_DIFF_FLAG_BINARY;
	else if (GIT_ATTR_TRUE(value))
		file->flags |= GIT_DIFF_FLAG_NOT_BINARY;
	/* otherwise leave file->flags alone */

	return 0;
}

static void update_delta_is_binary(git_diff_delta *delta)
{
	if ((delta->old_file.flags & GIT_DIFF_FLAG_BINARY) != 0 ||
		(delta->new_file.flags & GIT_DIFF_FLAG_BINARY) != 0)
		delta->flags |= GIT_DIFF_FLAG_BINARY;

	else if ((delta->old_file.flags & NOT_BINARY_FLAGS) != 0 &&
			 (delta->new_file.flags & NOT_BINARY_FLAGS) != 0)
		delta->flags |= GIT_DIFF_FLAG_NOT_BINARY;

	/* otherwise leave delta->flags binary value untouched */
}

/* returns if we forced binary setting (and no further checks needed) */
static bool diff_delta_is_binary_forced(
	diff_context *ctxt,
	git_diff_delta *delta)
{
	/* return true if binary-ness has already been settled */
	if ((delta->flags & KNOWN_BINARY_FLAGS) != 0)
		return true;

	/* make sure files are conceivably mmap-able */
	if ((git_off_t)((size_t)delta->old_file.size) != delta->old_file.size ||
		(git_off_t)((size_t)delta->new_file.size) != delta->new_file.size)
	{
		delta->old_file.flags |= GIT_DIFF_FLAG_BINARY;
		delta->new_file.flags |= GIT_DIFF_FLAG_BINARY;
		delta->flags |= GIT_DIFF_FLAG_BINARY;
		return true;
	}

	/* check if user is forcing us to text diff these files */
	if (ctxt->opts && (ctxt->opts->flags & GIT_DIFF_FORCE_TEXT) != 0) {
		delta->old_file.flags |= GIT_DIFF_FLAG_NOT_BINARY;
		delta->new_file.flags |= GIT_DIFF_FLAG_NOT_BINARY;
		delta->flags |= GIT_DIFF_FLAG_NOT_BINARY;
		return true;
	}

	return false;
}

static int diff_delta_is_binary_by_attr(
	diff_context *ctxt, git_diff_patch *patch)
{
	int error = 0, mirror_new;
	git_diff_delta *delta = patch->delta;

	if (diff_delta_is_binary_forced(ctxt, delta))
		return 0;

	/* check diff attribute +, -, or 0 */
	if (update_file_is_binary_by_attr(ctxt->repo, &delta->old_file) < 0)
		return -1;

	mirror_new = (delta->new_file.path == delta->old_file.path ||
		ctxt->diff->strcomp(delta->new_file.path, delta->old_file.path) == 0);
	if (mirror_new)
		delta->new_file.flags |= (delta->old_file.flags & KNOWN_BINARY_FLAGS);
	else
		error = update_file_is_binary_by_attr(ctxt->repo, &delta->new_file);

	update_delta_is_binary(delta);

	return error;
}

static int diff_delta_is_binary_by_content(
	diff_context *ctxt,
	git_diff_delta *delta,
	git_diff_file *file,
	const git_map *map)
{
	const git_buf search = { map->data, 0, min(map->len, 4000) };

	if (diff_delta_is_binary_forced(ctxt, delta))
		return 0;

	/* TODO: provide encoding / binary detection callbacks that can
	 * be UTF-8 aware, etc.  For now, instead of trying to be smart,
	 * let's just use the simple NUL-byte detection that core git uses.
	 */

	/* previously was: if (git_buf_text_is_binary(&search)) */
	if (git_buf_text_contains_nul(&search))
		file->flags |= GIT_DIFF_FLAG_BINARY;
	else
		file->flags |= GIT_DIFF_FLAG_NOT_BINARY;

	update_delta_is_binary(delta);

	return 0;
}

static int diff_delta_is_binary_by_size(
	diff_context *ctxt, git_diff_delta *delta, git_diff_file *file)
{
	git_off_t threshold = MAX_DIFF_FILESIZE;

	if ((file->flags & KNOWN_BINARY_FLAGS) != 0)
		return 0;

	if (ctxt && ctxt->opts) {
		if (ctxt->opts->max_size < 0)
			return 0;

		if (ctxt->opts->max_size > 0)
			threshold = ctxt->opts->max_size;
	}

	if (file->size > threshold)
		file->flags |= GIT_DIFF_FLAG_BINARY;

	update_delta_is_binary(delta);

	return 0;
}

static void setup_xdiff_options(
	const git_diff_options *opts, xdemitconf_t *cfg, xpparam_t *param)
{
	memset(cfg, 0, sizeof(xdemitconf_t));
	memset(param, 0, sizeof(xpparam_t));

	cfg->ctxlen =
		(!opts || !opts->context_lines) ? 3 : opts->context_lines;
	cfg->interhunkctxlen =
		(!opts) ? 0 : opts->interhunk_lines;

	if (!opts)
		return;

	if (opts->flags & GIT_DIFF_IGNORE_WHITESPACE)
		param->flags |= XDF_WHITESPACE_FLAGS;
	if (opts->flags & GIT_DIFF_IGNORE_WHITESPACE_CHANGE)
		param->flags |= XDF_IGNORE_WHITESPACE_CHANGE;
	if (opts->flags & GIT_DIFF_IGNORE_WHITESPACE_EOL)
		param->flags |= XDF_IGNORE_WHITESPACE_AT_EOL;
}


static int get_blob_content(
	diff_context *ctxt,
	git_diff_delta *delta,
	git_diff_file *file,
	git_map *map,
	git_blob **blob)
{
	int error;
	git_odb_object *odb_obj = NULL;

	if (git_oid_iszero(&file->oid))
		return 0;

	if (file->mode == GIT_FILEMODE_COMMIT)
	{
		char oidstr[GIT_OID_HEXSZ+1];
		git_buf content = GIT_BUF_INIT;

		git_oid_fmt(oidstr, &file->oid);
		oidstr[GIT_OID_HEXSZ] = 0;
		git_buf_printf(&content, "Subproject commit %s\n", oidstr );

		map->data = git_buf_detach(&content);
		map->len = strlen(map->data);

		file->flags |= GIT_DIFF_FLAG__FREE_DATA;
		return 0;
	}

	if (!file->size) {
		git_odb *odb;
		size_t len;
		git_otype type;

		/* peek at object header to avoid loading if too large */
		if ((error = git_repository_odb__weakptr(&odb, ctxt->repo)) < 0 ||
			(error = git_odb__read_header_or_object(
				&odb_obj, &len, &type, odb, &file->oid)) < 0)
			return error;

		assert(type == GIT_OBJ_BLOB);

		file->size = len;
	}

	/* if blob is too large to diff, mark as binary */
	if ((error = diff_delta_is_binary_by_size(ctxt, delta, file)) < 0)
		return error;
	if ((delta->flags & GIT_DIFF_FLAG_BINARY) != 0)
		return 0;

	if (odb_obj != NULL) {
		error = git_object__from_odb_object(
			(git_object **)blob, ctxt->repo, odb_obj, GIT_OBJ_BLOB);
		git_odb_object_free(odb_obj);
	} else
		error = git_blob_lookup(blob, ctxt->repo, &file->oid);

	if (error)
		return error;

	map->data = (void *)git_blob_rawcontent(*blob);
	map->len  = (size_t)git_blob_rawsize(*blob);

	return diff_delta_is_binary_by_content(ctxt, delta, file, map);
}

static int get_workdir_sm_content(
	diff_context *ctxt,
	git_diff_file *file,
	git_map *map)
{
	int error = 0;
	git_buf content = GIT_BUF_INIT;
	git_submodule* sm = NULL;
	unsigned int sm_status = 0;
	const char* sm_status_text = "";
	char oidstr[GIT_OID_HEXSZ+1];

	if ((error = git_submodule_lookup(&sm, ctxt->repo, file->path)) < 0 ||
		(error = git_submodule_status(&sm_status, sm)) < 0)
		return error;

	/* update OID if we didn't have it previously */
	if ((file->flags & GIT_DIFF_FLAG_VALID_OID) == 0) {
		const git_oid* sm_head;

		if ((sm_head = git_submodule_wd_id(sm)) != NULL ||
			(sm_head = git_submodule_head_id(sm)) != NULL)
		{
			git_oid_cpy(&file->oid, sm_head);
			file->flags |= GIT_DIFF_FLAG_VALID_OID;
		}
	}

	git_oid_fmt(oidstr, &file->oid);
	oidstr[GIT_OID_HEXSZ] = '\0';

	if (GIT_SUBMODULE_STATUS_IS_WD_DIRTY(sm_status))
		sm_status_text = "-dirty";

	git_buf_printf(&content, "Subproject commit %s%s\n",
				   oidstr, sm_status_text);

	map->data = git_buf_detach(&content);
	map->len = strlen(map->data);

	file->flags |= GIT_DIFF_FLAG__FREE_DATA;

	return 0;
}

static int get_workdir_content(
	diff_context *ctxt,
	git_diff_delta *delta,
	git_diff_file *file,
	git_map *map)
{
	int error = 0;
	git_buf path = GIT_BUF_INIT;
	const char *wd = git_repository_workdir(ctxt->repo);

	if (S_ISGITLINK(file->mode))
		return get_workdir_sm_content(ctxt, file, map);

	if (S_ISDIR(file->mode))
		return 0;

	if (git_buf_joinpath(&path, wd, file->path) < 0)
		return -1;

	if (S_ISLNK(file->mode)) {
		ssize_t alloc_len, read_len;

		file->flags |= GIT_DIFF_FLAG__FREE_DATA;
		file->flags |= GIT_DIFF_FLAG_BINARY;

		/* link path on disk could be UTF-16, so prepare a buffer that is
		 * big enough to handle some UTF-8 data expansion
		 */
		alloc_len = (ssize_t)(file->size * 2) + 1;

		map->data = git__malloc(alloc_len);
		GITERR_CHECK_ALLOC(map->data);

		read_len = p_readlink(path.ptr, map->data, (int)alloc_len);
		if (read_len < 0) {
			giterr_set(GITERR_OS, "Failed to read symlink '%s'", file->path);
			error = -1;
			goto cleanup;
		}

		map->len = read_len;
	}
	else {
		git_file fd = git_futils_open_ro(path.ptr);
		git_vector filters = GIT_VECTOR_INIT;

		if (fd < 0) {
			error = fd;
			goto cleanup;
		}

		if (!file->size)
			file->size = git_futils_filesize(fd);

		if ((error = diff_delta_is_binary_by_size(ctxt, delta, file)) < 0 ||
			(delta->flags & GIT_DIFF_FLAG_BINARY) != 0)
			goto close_and_cleanup;

		error = git_filters_load(
			&filters, ctxt->repo, file->path, GIT_FILTER_TO_ODB);
		if (error < 0)
			goto close_and_cleanup;

		if (error == 0) { /* note: git_filters_load returns filter count */
			if (!file->size)
				goto close_and_cleanup;

			error = git_futils_mmap_ro(map, fd, 0, (size_t)file->size);
			file->flags |= GIT_DIFF_FLAG__UNMAP_DATA;
		} else {
			git_buf raw = GIT_BUF_INIT, filtered = GIT_BUF_INIT;

			if (!(error = git_futils_readbuffer_fd(&raw, fd, (size_t)file->size)) &&
				!(error = git_filters_apply(&filtered, &raw, &filters)))
			{
				map->len  = git_buf_len(&filtered);
				map->data = git_buf_detach(&filtered);

				file->flags |= GIT_DIFF_FLAG__FREE_DATA;
			}

			git_buf_free(&raw);
			git_buf_free(&filtered);
		}

close_and_cleanup:
		git_filters_free(&filters);
		p_close(fd);
	}

	/* once data is loaded, update OID if we didn't have it previously */
	if (!error && (file->flags & GIT_DIFF_FLAG_VALID_OID) == 0) {
		error = git_odb_hash(
			&file->oid, map->data, map->len, GIT_OBJ_BLOB);
		if (!error)
			file->flags |= GIT_DIFF_FLAG_VALID_OID;
	}

	if (!error)
		error = diff_delta_is_binary_by_content(ctxt, delta, file, map);

cleanup:
	git_buf_free(&path);
	return error;
}

static void release_content(git_diff_file *file, git_map *map, git_blob *blob)
{
	if (blob != NULL)
		git_blob_free(blob);

	if (file->flags & GIT_DIFF_FLAG__FREE_DATA) {
		git__free(map->data);
		map->data = "";
		map->len  = 0;
		file->flags &= ~GIT_DIFF_FLAG__FREE_DATA;
	}
	else if (file->flags & GIT_DIFF_FLAG__UNMAP_DATA) {
		git_futils_mmap_free(map);
		map->data = "";
		map->len  = 0;
		file->flags &= ~GIT_DIFF_FLAG__UNMAP_DATA;
	}
}


static int diff_context_init(
	diff_context *ctxt,
	git_diff_list *diff,
	git_repository *repo,
	const git_diff_options *opts,
	git_diff_file_cb file_cb,
	git_diff_hunk_cb hunk_cb,
	git_diff_data_cb data_cb,
	void *payload)
{
	memset(ctxt, 0, sizeof(diff_context));

	if (!repo && diff)
		repo = diff->repo;

	if (!opts && diff)
		opts = &diff->opts;

	ctxt->repo = repo;
	ctxt->diff = diff;
	ctxt->opts = opts;
	ctxt->file_cb = file_cb;
	ctxt->hunk_cb = hunk_cb;
	ctxt->data_cb = data_cb;
	ctxt->payload = payload;
	ctxt->error = 0;

	setup_xdiff_options(ctxt->opts, &ctxt->xdiff_config, &ctxt->xdiff_params);

	return 0;
}

static int diff_delta_file_callback(
	diff_context *ctxt, git_diff_delta *delta, size_t idx)
{
	float progress;

	if (!ctxt->file_cb)
		return 0;

	progress = ctxt->diff ? ((float)idx / ctxt->diff->deltas.length) : 1.0f;

	if (ctxt->file_cb(delta, progress, ctxt->payload) != 0)
		ctxt->error = GIT_EUSER;

	return ctxt->error;
}

static void diff_patch_init(
	diff_context *ctxt, git_diff_patch *patch)
{
	memset(patch, 0, sizeof(git_diff_patch));

	patch->diff = ctxt->diff;
	patch->ctxt = ctxt;

	if (patch->diff) {
		patch->old_src = patch->diff->old_src;
		patch->new_src = patch->diff->new_src;
	} else {
		patch->old_src = patch->new_src = GIT_ITERATOR_TYPE_TREE;
	}
}

static git_diff_patch *diff_patch_alloc(
	diff_context *ctxt, git_diff_delta *delta)
{
	git_diff_patch *patch = git__malloc(sizeof(git_diff_patch));
	if (!patch)
		return NULL;

	diff_patch_init(ctxt, patch);

	git_diff_list_addref(patch->diff);

	GIT_REFCOUNT_INC(patch);

	patch->delta = delta;
	patch->flags = GIT_DIFF_PATCH_ALLOCATED;

	return patch;
}

static int diff_patch_load(
	diff_context *ctxt, git_diff_patch *patch)
{
	int error = 0;
	git_diff_delta *delta = patch->delta;
	bool check_if_unmodified = false;

	if ((patch->flags & GIT_DIFF_PATCH_LOADED) != 0)
		return 0;

	error = diff_delta_is_binary_by_attr(ctxt, patch);

	patch->old_data.data = "";
	patch->old_data.len  = 0;
	patch->old_blob      = NULL;

	patch->new_data.data = "";
	patch->new_data.len  = 0;
	patch->new_blob      = NULL;

	if ((delta->flags & GIT_DIFF_FLAG_BINARY) != 0)
		goto cleanup;

	if (!ctxt->hunk_cb &&
		!ctxt->data_cb &&
		(ctxt->opts->flags & GIT_DIFF_SKIP_BINARY_CHECK) != 0)
		goto cleanup;

	switch (delta->status) {
	case GIT_DELTA_ADDED:
		delta->old_file.flags |= GIT_DIFF_FLAG__NO_DATA;
		break;
	case GIT_DELTA_DELETED:
		delta->new_file.flags |= GIT_DIFF_FLAG__NO_DATA;
		break;
	case GIT_DELTA_MODIFIED:
		break;
	case GIT_DELTA_UNTRACKED:
		delta->old_file.flags |= GIT_DIFF_FLAG__NO_DATA;
		if ((ctxt->opts->flags & GIT_DIFF_INCLUDE_UNTRACKED_CONTENT) == 0)
			delta->new_file.flags |= GIT_DIFF_FLAG__NO_DATA;
		break;
	default:
		delta->new_file.flags |= GIT_DIFF_FLAG__NO_DATA;
		delta->old_file.flags |= GIT_DIFF_FLAG__NO_DATA;
		break;
	}

#define CHECK_UNMODIFIED (GIT_DIFF_FLAG__NO_DATA | GIT_DIFF_FLAG_VALID_OID)

	check_if_unmodified =
		(delta->old_file.flags & CHECK_UNMODIFIED) == 0 &&
		(delta->new_file.flags & CHECK_UNMODIFIED) == 0;

	/* Always try to load workdir content first, since it may need to be
	 * filtered (and hence use 2x memory) and we want to minimize the max
	 * memory footprint during diff.
	 */

	if ((delta->old_file.flags & GIT_DIFF_FLAG__NO_DATA) == 0 &&
		patch->old_src == GIT_ITERATOR_TYPE_WORKDIR) {
		if ((error = get_workdir_content(
				ctxt, delta, &delta->old_file, &patch->old_data)) < 0)
			goto cleanup;
		if ((delta->flags & GIT_DIFF_FLAG_BINARY) != 0)
			goto cleanup;
	}

	if ((delta->new_file.flags & GIT_DIFF_FLAG__NO_DATA) == 0 &&
		patch->new_src == GIT_ITERATOR_TYPE_WORKDIR) {
		if ((error = get_workdir_content(
				ctxt, delta, &delta->new_file, &patch->new_data)) < 0)
			goto cleanup;
		if ((delta->flags & GIT_DIFF_FLAG_BINARY) != 0)
			goto cleanup;
	}

	if ((delta->old_file.flags & GIT_DIFF_FLAG__NO_DATA) == 0 &&
		patch->old_src != GIT_ITERATOR_TYPE_WORKDIR) {
		if ((error = get_blob_content(
				ctxt, delta, &delta->old_file,
				&patch->old_data, &patch->old_blob)) < 0)
			goto cleanup;
		if ((delta->flags & GIT_DIFF_FLAG_BINARY) != 0)
			goto cleanup;
	}

	if ((delta->new_file.flags & GIT_DIFF_FLAG__NO_DATA) == 0 &&
		patch->new_src != GIT_ITERATOR_TYPE_WORKDIR) {
		if ((error = get_blob_content(
				ctxt, delta, &delta->new_file,
				&patch->new_data, &patch->new_blob)) < 0)
			goto cleanup;
		if ((delta->flags & GIT_DIFF_FLAG_BINARY) != 0)
			goto cleanup;
	}

	/* if we did not previously have the definitive oid, we may have
	 * incorrect status and need to switch this to UNMODIFIED.
	 */
	if (check_if_unmodified &&
		delta->old_file.mode == delta->new_file.mode &&
		!git_oid_cmp(&delta->old_file.oid, &delta->new_file.oid))
	{
		delta->status = GIT_DELTA_UNMODIFIED;

		if ((ctxt->opts->flags & GIT_DIFF_INCLUDE_UNMODIFIED) == 0)
			goto cleanup;
	}

cleanup:
	if ((delta->flags & KNOWN_BINARY_FLAGS) == 0)
		update_delta_is_binary(delta);

	if (!error) {
		patch->flags |= GIT_DIFF_PATCH_LOADED;

		if ((delta->flags & GIT_DIFF_FLAG_BINARY) == 0 &&
			delta->status != GIT_DELTA_UNMODIFIED &&
			(patch->old_data.len || patch->new_data.len) &&
			!git_oid_equal(&delta->old_file.oid, &delta->new_file.oid))
			patch->flags |= GIT_DIFF_PATCH_DIFFABLE;
	}

	return error;
}

static int diff_patch_cb(void *priv, mmbuffer_t *bufs, int len)
{
	git_diff_patch *patch = priv;
	diff_context   *ctxt  = patch->ctxt;

	if (len == 1) {
		ctxt->error = parse_hunk_header(&ctxt->range, bufs[0].ptr);
		if (ctxt->error < 0)
			return ctxt->error;

		if (ctxt->hunk_cb != NULL &&
			ctxt->hunk_cb(patch->delta, &ctxt->range,
				bufs[0].ptr, bufs[0].size, ctxt->payload))
			ctxt->error = GIT_EUSER;
	}

	if (len == 2 || len == 3) {
		/* expect " "/"-"/"+", then data */
		char origin =
			(*bufs[0].ptr == '+') ? GIT_DIFF_LINE_ADDITION :
			(*bufs[0].ptr == '-') ? GIT_DIFF_LINE_DELETION :
			GIT_DIFF_LINE_CONTEXT;

		if (ctxt->data_cb != NULL &&
			ctxt->data_cb(patch->delta, &ctxt->range,
				origin, bufs[1].ptr, bufs[1].size, ctxt->payload))
			ctxt->error = GIT_EUSER;
	}

	if (len == 3 && !ctxt->error) {
		/* If we have a '+' and a third buf, then we have added a line
		 * without a newline and the old code had one, so DEL_EOFNL.
		 * If we have a '-' and a third buf, then we have removed a line
		 * with out a newline but added a blank line, so ADD_EOFNL.
		 */
		char origin =
			(*bufs[0].ptr == '+') ? GIT_DIFF_LINE_DEL_EOFNL :
			(*bufs[0].ptr == '-') ? GIT_DIFF_LINE_ADD_EOFNL :
			GIT_DIFF_LINE_CONTEXT;

		if (ctxt->data_cb != NULL &&
			ctxt->data_cb(patch->delta, &ctxt->range,
				origin, bufs[2].ptr, bufs[2].size, ctxt->payload))
			ctxt->error = GIT_EUSER;
	}

	return ctxt->error;
}

static int diff_patch_generate(
	diff_context *ctxt, git_diff_patch *patch)
{
	int error = 0;
	xdemitcb_t xdiff_callback;
	mmfile_t old_xdiff_data, new_xdiff_data;

	if ((patch->flags & GIT_DIFF_PATCH_DIFFED) != 0)
		return 0;

	if ((patch->flags & GIT_DIFF_PATCH_LOADED) == 0)
		if ((error = diff_patch_load(ctxt, patch)) < 0)
			return error;

	if ((patch->flags & GIT_DIFF_PATCH_DIFFABLE) == 0)
		return 0;

	if (!ctxt->file_cb && !ctxt->hunk_cb)
		return 0;

	patch->ctxt = ctxt;

	memset(&xdiff_callback, 0, sizeof(xdiff_callback));
	xdiff_callback.outf = diff_patch_cb;
	xdiff_callback.priv = patch;

	old_xdiff_data.ptr  = patch->old_data.data;
	old_xdiff_data.size = patch->old_data.len;
	new_xdiff_data.ptr  = patch->new_data.data;
	new_xdiff_data.size = patch->new_data.len;

	xdl_diff(&old_xdiff_data, &new_xdiff_data,
		&ctxt->xdiff_params, &ctxt->xdiff_config, &xdiff_callback);

	error = ctxt->error;

	if (!error)
		patch->flags |= GIT_DIFF_PATCH_DIFFED;

	return error;
}

static void diff_patch_unload(git_diff_patch *patch)
{
	if ((patch->flags & GIT_DIFF_PATCH_DIFFED) != 0) {
		patch->flags = (patch->flags & ~GIT_DIFF_PATCH_DIFFED);

		patch->hunks_size = 0;
		patch->lines_size = 0;
	}

	if ((patch->flags & GIT_DIFF_PATCH_LOADED) != 0) {
		patch->flags = (patch->flags & ~GIT_DIFF_PATCH_LOADED);

		release_content(
			&patch->delta->old_file, &patch->old_data, patch->old_blob);
		release_content(
			&patch->delta->new_file, &patch->new_data, patch->new_blob);
	}
}

static void diff_patch_free(git_diff_patch *patch)
{
	diff_patch_unload(patch);

	git__free(patch->lines);
	patch->lines = NULL;
	patch->lines_asize = 0;

	git__free(patch->hunks);
	patch->hunks = NULL;
	patch->hunks_asize = 0;

	if (!(patch->flags & GIT_DIFF_PATCH_ALLOCATED))
		return;

	patch->flags = 0;

	git_diff_list_free(patch->diff); /* decrements refcount */

	git__free(patch);
}

#define MAX_HUNK_STEP 128
#define MIN_HUNK_STEP 8
#define MAX_LINE_STEP 256
#define MIN_LINE_STEP 8

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

	if (patch->hunks_size >= patch->hunks_asize) {
		size_t new_size;
		diff_patch_hunk *new_hunks;

		if (patch->hunks_asize > MAX_HUNK_STEP)
			new_size = patch->hunks_asize + MAX_HUNK_STEP;
		else
			new_size = patch->hunks_asize * 2;
		if (new_size < MIN_HUNK_STEP)
			new_size = MIN_HUNK_STEP;

		new_hunks = git__realloc(
			patch->hunks, new_size * sizeof(diff_patch_hunk));
		if (!new_hunks)
			return -1;

		patch->hunks = new_hunks;
		patch->hunks_asize = new_size;
	}

	hunk = &patch->hunks[patch->hunks_size++];

	memcpy(&hunk->range, range, sizeof(hunk->range));

	assert(header_len + 1 < sizeof(hunk->header));
	memcpy(&hunk->header, header, header_len);
	hunk->header[header_len] = '\0';
	hunk->header_len = header_len;

	hunk->line_start = patch->lines_size;
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

	assert(patch->hunks_size > 0);
	assert(patch->hunks != NULL);

	hunk = &patch->hunks[patch->hunks_size - 1];

	if (patch->lines_size >= patch->lines_asize) {
		size_t new_size;
		diff_patch_line *new_lines;

		if (patch->lines_asize > MAX_LINE_STEP)
			new_size = patch->lines_asize + MAX_LINE_STEP;
		else
			new_size = patch->lines_asize * 2;
		if (new_size < MIN_LINE_STEP)
			new_size = MIN_LINE_STEP;

		new_lines = git__realloc(
			patch->lines, new_size * sizeof(diff_patch_line));
		if (!new_lines)
			return -1;

		patch->lines = new_lines;
		patch->lines_asize = new_size;
	}

	line = &patch->lines[patch->lines_size++];

	line->ptr = content;
	line->len = content_len;
	line->origin = line_origin;

	/* do some bookkeeping so we can provide old/new line numbers */

	for (line->lines = 0; content_len > 0; --content_len) {
		if (*content++ == '\n')
			++line->lines;
	}

	switch (line_origin) {
	case GIT_DIFF_LINE_ADDITION:
		line->oldno = -1;
		line->newno = patch->newno;
		patch->newno += line->lines;
		break;
	case GIT_DIFF_LINE_DELETION:
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

static int diff_required(git_diff_list *diff, const char *action)
{
	if (!diff) {
		giterr_set(GITERR_INVALID, "Must provide valid diff to %s", action);
		return -1;
	}

	return 0;
}

int git_diff_foreach(
	git_diff_list *diff,
	git_diff_file_cb file_cb,
	git_diff_hunk_cb hunk_cb,
	git_diff_data_cb data_cb,
	void *payload)
{
	int error = 0;
	diff_context ctxt;
	size_t idx;
	git_diff_patch patch;

	if (diff_required(diff, "git_diff_foreach") < 0)
		return -1;

	if (diff_context_init(
			&ctxt, diff, NULL, NULL, file_cb, hunk_cb, data_cb, payload) < 0)
		return -1;

	diff_patch_init(&ctxt, &patch);

	git_vector_foreach(&diff->deltas, idx, patch.delta) {

		/* check flags against patch status */
		if (git_diff_delta__should_skip(ctxt.opts, patch.delta))
			continue;

		if (!(error = diff_patch_load(&ctxt, &patch))) {

			/* invoke file callback */
			error = diff_delta_file_callback(&ctxt, patch.delta, idx);

			/* generate diffs and invoke hunk and line callbacks */
			if (!error)
				error = diff_patch_generate(&ctxt, &patch);

			diff_patch_unload(&patch);
		}

		if (error < 0)
			break;
	}

	if (error == GIT_EUSER)
		giterr_clear(); /* don't let error message leak */

	return error;
}


typedef struct {
	git_diff_list *diff;
	git_diff_data_cb print_cb;
	void *payload;
	git_buf *buf;
} diff_print_info;

static char pick_suffix(int mode)
{
	if (S_ISDIR(mode))
		return '/';
	else if (mode & 0100) //-V536
		/* in git, modes are very regular, so we must have 0100755 mode */
		return '*';
	else
		return ' ';
}

char git_diff_status_char(git_delta_t status)
{
	char code;

	switch (status) {
	case GIT_DELTA_ADDED:     code = 'A'; break;
	case GIT_DELTA_DELETED:   code = 'D'; break;
	case GIT_DELTA_MODIFIED:  code = 'M'; break;
	case GIT_DELTA_RENAMED:   code = 'R'; break;
	case GIT_DELTA_COPIED:    code = 'C'; break;
	case GIT_DELTA_IGNORED:   code = 'I'; break;
	case GIT_DELTA_UNTRACKED: code = '?'; break;
	default:                  code = ' '; break;
	}

	return code;
}

static int print_compact(
	const git_diff_delta *delta, float progress, void *data)
{
	diff_print_info *pi = data;
	char old_suffix, new_suffix, code = git_diff_status_char(delta->status);

	GIT_UNUSED(progress);

	if (code == ' ')
		return 0;

	old_suffix = pick_suffix(delta->old_file.mode);
	new_suffix = pick_suffix(delta->new_file.mode);

	git_buf_clear(pi->buf);

	if (delta->old_file.path != delta->new_file.path &&
		pi->diff->strcomp(delta->old_file.path,delta->new_file.path) != 0)
		git_buf_printf(pi->buf, "%c\t%s%c -> %s%c\n", code,
			delta->old_file.path, old_suffix, delta->new_file.path, new_suffix);
	else if (delta->old_file.mode != delta->new_file.mode &&
		delta->old_file.mode != 0 && delta->new_file.mode != 0)
		git_buf_printf(pi->buf, "%c\t%s%c (%o -> %o)\n", code,
			delta->old_file.path, new_suffix, delta->old_file.mode, delta->new_file.mode);
	else if (old_suffix != ' ')
		git_buf_printf(pi->buf, "%c\t%s%c\n", code, delta->old_file.path, old_suffix);
	else
		git_buf_printf(pi->buf, "%c\t%s\n", code, delta->old_file.path);

	if (git_buf_oom(pi->buf))
		return -1;

	if (pi->print_cb(delta, NULL, GIT_DIFF_LINE_FILE_HDR,
			git_buf_cstr(pi->buf), git_buf_len(pi->buf), pi->payload))
	{
		giterr_clear();
		return GIT_EUSER;
	}

	return 0;
}

int git_diff_print_compact(
	git_diff_list *diff,
	git_diff_data_cb print_cb,
	void *payload)
{
	int error;
	git_buf buf = GIT_BUF_INIT;
	diff_print_info pi;

	pi.diff     = diff;
	pi.print_cb = print_cb;
	pi.payload  = payload;
	pi.buf      = &buf;

	error = git_diff_foreach(diff, print_compact, NULL, NULL, &pi);

	git_buf_free(&buf);

	return error;
}

static int print_oid_range(diff_print_info *pi, const git_diff_delta *delta)
{
	char start_oid[8], end_oid[8];

	/* TODO: Determine a good actual OID range to print */
	git_oid_tostr(start_oid, sizeof(start_oid), &delta->old_file.oid);
	git_oid_tostr(end_oid, sizeof(end_oid), &delta->new_file.oid);

	/* TODO: Match git diff more closely */
	if (delta->old_file.mode == delta->new_file.mode) {
		git_buf_printf(pi->buf, "index %s..%s %o\n",
			start_oid, end_oid, delta->old_file.mode);
	} else {
		if (delta->old_file.mode == 0) {
			git_buf_printf(pi->buf, "new file mode %o\n", delta->new_file.mode);
		} else if (delta->new_file.mode == 0) {
			git_buf_printf(pi->buf, "deleted file mode %o\n", delta->old_file.mode);
		} else {
			git_buf_printf(pi->buf, "old mode %o\n", delta->old_file.mode);
			git_buf_printf(pi->buf, "new mode %o\n", delta->new_file.mode);
		}
		git_buf_printf(pi->buf, "index %s..%s\n", start_oid, end_oid);
	}

	if (git_buf_oom(pi->buf))
		return -1;

	return 0;
}

static int print_patch_file(
	const git_diff_delta *delta, float progress, void *data)
{
	diff_print_info *pi = data;
	const char *oldpfx = pi->diff->opts.old_prefix;
	const char *oldpath = delta->old_file.path;
	const char *newpfx = pi->diff->opts.new_prefix;
	const char *newpath = delta->new_file.path;

	GIT_UNUSED(progress);

	if (S_ISDIR(delta->new_file.mode))
		return 0;

	if (!oldpfx)
		oldpfx = DIFF_OLD_PREFIX_DEFAULT;

	if (!newpfx)
		newpfx = DIFF_NEW_PREFIX_DEFAULT;

	git_buf_clear(pi->buf);
	git_buf_printf(pi->buf, "diff --git %s%s %s%s\n", oldpfx, delta->old_file.path, newpfx, delta->new_file.path);

	if (print_oid_range(pi, delta) < 0)
		return -1;

	if (git_oid_iszero(&delta->old_file.oid)) {
		oldpfx = "";
		oldpath = "/dev/null";
	}
	if (git_oid_iszero(&delta->new_file.oid)) {
		newpfx = "";
		newpath = "/dev/null";
	}

	if ((delta->flags & GIT_DIFF_FLAG_BINARY) == 0) {
		git_buf_printf(pi->buf, "--- %s%s\n", oldpfx, oldpath);
		git_buf_printf(pi->buf, "+++ %s%s\n", newpfx, newpath);
	}

	if (git_buf_oom(pi->buf))
		return -1;

	if (pi->print_cb(delta, NULL, GIT_DIFF_LINE_FILE_HDR,
			git_buf_cstr(pi->buf), git_buf_len(pi->buf), pi->payload))
	{
		giterr_clear();
		return GIT_EUSER;
	}

	if ((delta->flags & GIT_DIFF_FLAG_BINARY) == 0)
		return 0;

	git_buf_clear(pi->buf);
	git_buf_printf(
		pi->buf, "Binary files %s%s and %s%s differ\n",
		oldpfx, oldpath, newpfx, newpath);
	if (git_buf_oom(pi->buf))
		return -1;

	if (pi->print_cb(delta, NULL, GIT_DIFF_LINE_BINARY,
			git_buf_cstr(pi->buf), git_buf_len(pi->buf), pi->payload))
	{
		giterr_clear();
		return GIT_EUSER;
	}

	return 0;
}

static int print_patch_hunk(
	const git_diff_delta *d,
	const git_diff_range *r,
	const char *header,
	size_t header_len,
	void *data)
{
	diff_print_info *pi = data;

	if (S_ISDIR(d->new_file.mode))
		return 0;

	git_buf_clear(pi->buf);
	if (git_buf_printf(pi->buf, "%.*s", (int)header_len, header) < 0)
		return -1;

	if (pi->print_cb(d, r, GIT_DIFF_LINE_HUNK_HDR,
			git_buf_cstr(pi->buf), git_buf_len(pi->buf), pi->payload))
	{
		giterr_clear();
		return GIT_EUSER;
	}

	return 0;
}

static int print_patch_line(
	const git_diff_delta *delta,
	const git_diff_range *range,
	char line_origin, /* GIT_DIFF_LINE value from above */
	const char *content,
	size_t content_len,
	void *data)
{
	diff_print_info *pi = data;

	if (S_ISDIR(delta->new_file.mode))
		return 0;

	git_buf_clear(pi->buf);

	if (line_origin == GIT_DIFF_LINE_ADDITION ||
		line_origin == GIT_DIFF_LINE_DELETION ||
		line_origin == GIT_DIFF_LINE_CONTEXT)
		git_buf_printf(pi->buf, "%c%.*s", line_origin, (int)content_len, content);
	else if (content_len > 0)
		git_buf_printf(pi->buf, "%.*s", (int)content_len, content);

	if (git_buf_oom(pi->buf))
		return -1;

	if (pi->print_cb(delta, range, line_origin,
			git_buf_cstr(pi->buf), git_buf_len(pi->buf), pi->payload))
	{
		giterr_clear();
		return GIT_EUSER;
	}

	return 0;
}

int git_diff_print_patch(
	git_diff_list *diff,
	git_diff_data_cb print_cb,
	void *payload)
{
	int error;
	git_buf buf = GIT_BUF_INIT;
	diff_print_info pi;

	pi.diff     = diff;
	pi.print_cb = print_cb;
	pi.payload  = payload;
	pi.buf      = &buf;

	error = git_diff_foreach(
		diff, print_patch_file, print_patch_hunk, print_patch_line, &pi);

	git_buf_free(&buf);

	return error;
}

static void set_data_from_blob(
	const git_blob *blob, git_map *map, git_diff_file *file)
{
	if (blob) {
		file->size = git_blob_rawsize(blob);
		git_oid_cpy(&file->oid, git_object_id((const git_object *)blob));
		file->mode = 0644;

		map->len   = (size_t)file->size;
		map->data  = (char *)git_blob_rawcontent(blob);
	} else {
		file->size = 0;
		file->flags |= GIT_DIFF_FLAG__NO_DATA;

		map->len   = 0;
		map->data  = "";
	}
}

static void set_data_from_buffer(
	const char *buffer, size_t buffer_len, git_map *map, git_diff_file *file)
{
	file->size = (git_off_t)buffer_len;
	file->mode = 0644;
	map->len   = buffer_len;

	if (!buffer) {
		file->flags |= GIT_DIFF_FLAG__NO_DATA;
		map->data = NULL;
	} else {
		map->data = (char *)buffer;
		git_odb_hash(&file->oid, buffer, buffer_len, GIT_OBJ_BLOB);
	}
}

typedef struct {
	diff_context   ctxt;
	git_diff_delta delta;
	git_diff_patch patch;
} diff_single_data;

static int diff_single_init(
	diff_single_data *data,
	git_repository *repo,
	const git_diff_options *opts,
	git_diff_file_cb file_cb,
	git_diff_hunk_cb hunk_cb,
	git_diff_data_cb data_cb,
	void *payload)
{
	GITERR_CHECK_VERSION(opts, GIT_DIFF_OPTIONS_VERSION, "git_diff_options");

	memset(data, 0, sizeof(*data));

	if (diff_context_init(
			&data->ctxt, NULL, repo, opts,
			file_cb, hunk_cb, data_cb, payload) < 0)
		return -1;

	diff_patch_init(&data->ctxt, &data->patch);

	return 0;
}

static int diff_single_apply(diff_single_data *data)
{
	int error;
	git_diff_delta *delta = &data->delta;
	bool has_old = ((delta->old_file.flags & GIT_DIFF_FLAG__NO_DATA) == 0);
	bool has_new = ((delta->new_file.flags & GIT_DIFF_FLAG__NO_DATA) == 0);

	/* finish setting up fake git_diff_delta record and loaded data */

	data->patch.delta = delta;
	delta->flags = delta->flags & ~KNOWN_BINARY_FLAGS;

	delta->status = has_new ?
		(has_old ? GIT_DELTA_MODIFIED : GIT_DELTA_ADDED) :
		(has_old ? GIT_DELTA_DELETED : GIT_DELTA_UNTRACKED);

	if (git_oid_cmp(&delta->new_file.oid, &delta->old_file.oid) == 0)
		delta->status = GIT_DELTA_UNMODIFIED;

	if ((error = diff_delta_is_binary_by_content(
			&data->ctxt, delta, &delta->old_file, &data->patch.old_data)) < 0 ||
		(error = diff_delta_is_binary_by_content(
			&data->ctxt, delta, &delta->new_file, &data->patch.new_data)) < 0)
		goto cleanup;

	data->patch.flags |= GIT_DIFF_PATCH_LOADED;

	if ((delta->flags & GIT_DIFF_FLAG_BINARY) == 0 &&
		delta->status != GIT_DELTA_UNMODIFIED)
		data->patch.flags |= GIT_DIFF_PATCH_DIFFABLE;

	/* do diffs */

	if (!(error = diff_delta_file_callback(&data->ctxt, delta, 1)))
		error = diff_patch_generate(&data->ctxt, &data->patch);

cleanup:
	if (error == GIT_EUSER)
		giterr_clear();

	diff_patch_unload(&data->patch);

	return error;
}

int git_diff_blobs(
	const git_blob *old_blob,
	const git_blob *new_blob,
	const git_diff_options *options,
	git_diff_file_cb file_cb,
	git_diff_hunk_cb hunk_cb,
	git_diff_data_cb data_cb,
	void *payload)
{
	int error;
	diff_single_data d;
	git_repository *repo =
		new_blob ? git_object_owner((const git_object *)new_blob) :
		old_blob ? git_object_owner((const git_object *)old_blob) : NULL;

	if (!repo) /* Hmm, given two NULL blobs, silently do no callbacks? */
		return 0;

	if ((error = diff_single_init(
			&d, repo, options, file_cb, hunk_cb, data_cb, payload)) < 0)
		return error;

	if (options && (options->flags & GIT_DIFF_REVERSE) != 0) {
		const git_blob *swap = old_blob;
		old_blob = new_blob;
		new_blob = swap;
	}

	set_data_from_blob(old_blob, &d.patch.old_data, &d.delta.old_file);
	set_data_from_blob(new_blob, &d.patch.new_data, &d.delta.new_file);

	return diff_single_apply(&d);
}

int git_diff_blob_to_buffer(
	const git_blob *old_blob,
	const char *buf,
	size_t buflen,
	const git_diff_options *options,
	git_diff_file_cb file_cb,
	git_diff_hunk_cb hunk_cb,
	git_diff_data_cb data_cb,
	void *payload)
{
	int error;
	diff_single_data d;
	git_repository *repo =
		old_blob ? git_object_owner((const git_object *)old_blob) : NULL;

	if (!repo && !buf) /* Hmm, given NULLs, silently do no callbacks? */
		return 0;

	if ((error = diff_single_init(
			&d, repo, options, file_cb, hunk_cb, data_cb, payload)) < 0)
		return error;

	if (options && (options->flags & GIT_DIFF_REVERSE) != 0) {
		set_data_from_buffer(buf, buflen, &d.patch.old_data, &d.delta.old_file);
		set_data_from_blob(old_blob, &d.patch.new_data, &d.delta.new_file);
	} else {
		set_data_from_blob(old_blob, &d.patch.old_data, &d.delta.old_file);
		set_data_from_buffer(buf, buflen, &d.patch.new_data, &d.delta.new_file);
	}

	return diff_single_apply(&d);
}

size_t git_diff_num_deltas(git_diff_list *diff)
{
	assert(diff);
	return (size_t)diff->deltas.length;
}

size_t git_diff_num_deltas_of_type(git_diff_list *diff, git_delta_t type)
{
	size_t i, count = 0;
	git_diff_delta *delta;

	assert(diff);

	git_vector_foreach(&diff->deltas, i, delta) {
		count += (delta->status == type);
	}

	return count;
}

int git_diff_get_patch(
	git_diff_patch **patch_ptr,
	const git_diff_delta **delta_ptr,
	git_diff_list *diff,
	size_t idx)
{
	int error;
	diff_context ctxt;
	git_diff_delta *delta;
	git_diff_patch *patch;

	if (patch_ptr)
		*patch_ptr = NULL;
	if (delta_ptr)
		*delta_ptr = NULL;

	if (diff_required(diff, "git_diff_get_patch") < 0)
		return -1;

	if (diff_context_init(
			&ctxt, diff, NULL, NULL,
			NULL, diff_patch_hunk_cb, diff_patch_line_cb, NULL) < 0)
		return -1;

	delta = git_vector_get(&diff->deltas, idx);
	if (!delta) {
		giterr_set(GITERR_INVALID, "Index out of range for delta in diff");
		return GIT_ENOTFOUND;
	}

	if (delta_ptr)
		*delta_ptr = delta;

	if (!patch_ptr &&
		((delta->flags & KNOWN_BINARY_FLAGS) != 0 ||
		 (diff->opts.flags & GIT_DIFF_SKIP_BINARY_CHECK) != 0))
		return 0;

	if (git_diff_delta__should_skip(ctxt.opts, delta))
		return 0;

	patch = diff_patch_alloc(&ctxt, delta);
	if (!patch)
		return -1;

	if (!(error = diff_patch_load(&ctxt, patch))) {
		ctxt.payload = patch;

		error = diff_patch_generate(&ctxt, patch);

		if (error == GIT_EUSER)
			error = ctxt.error;
	}

	if (error)
		git_diff_patch_free(patch);
	else if (patch_ptr)
		*patch_ptr = patch;

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
	return patch->hunks_size;
}

int git_diff_patch_line_stats(
	size_t *total_ctxt,
	size_t *total_adds,
	size_t *total_dels,
	const git_diff_patch *patch)
{
	size_t totals[3], idx;

	memset(totals, 0, sizeof(totals));

	for (idx = 0; idx < patch->lines_size; ++idx) {
		switch (patch->lines[idx].origin) {
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

	if (hunk_idx >= patch->hunks_size) {
		if (range) *range = NULL;
		if (header) *header = NULL;
		if (header_len) *header_len = 0;
		if (lines_in_hunk) *lines_in_hunk = 0;
		return GIT_ENOTFOUND;
	}

	hunk = &patch->hunks[hunk_idx];

	if (range) *range = &hunk->range;
	if (header) *header = hunk->header;
	if (header_len) *header_len = hunk->header_len;
	if (lines_in_hunk) *lines_in_hunk = hunk->line_count;

	return 0;
}

int git_diff_patch_num_lines_in_hunk(
	git_diff_patch *patch,
	size_t hunk_idx)
{
	assert(patch);

	if (hunk_idx >= patch->hunks_size)
		return GIT_ENOTFOUND;
	else
		return (int)patch->hunks[hunk_idx].line_count;
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

	assert(patch);

	if (hunk_idx >= patch->hunks_size)
		goto notfound;
	hunk = &patch->hunks[hunk_idx];

	if (line_of_hunk >= hunk->line_count)
		goto notfound;

	line = &patch->lines[hunk->line_start + line_of_hunk];

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

	return GIT_ENOTFOUND;
}

static int print_to_buffer_cb(
    const git_diff_delta *delta,
    const git_diff_range *range,
    char line_origin,
    const char *content,
    size_t content_len,
    void *payload)
{
	git_buf *output = payload;
	GIT_UNUSED(delta); GIT_UNUSED(range); GIT_UNUSED(line_origin);
	return git_buf_put(output, content, content_len);
}

int git_diff_patch_print(
	git_diff_patch *patch,
	git_diff_data_cb print_cb,
	void *payload)
{
	int error;
	git_buf temp = GIT_BUF_INIT;
	diff_print_info pi;
	size_t h, l;

	assert(patch && print_cb);

	pi.diff     = patch->diff;
	pi.print_cb = print_cb;
	pi.payload  = payload;
	pi.buf      = &temp;

	error = print_patch_file(patch->delta, 0, &pi);

	for (h = 0; h < patch->hunks_size && !error; ++h) {
		diff_patch_hunk *hunk = &patch->hunks[h];

		error = print_patch_hunk(
			patch->delta, &hunk->range, hunk->header, hunk->header_len, &pi);

		for (l = 0; l < hunk->line_count && !error; ++l) {
			diff_patch_line *line = &patch->lines[hunk->line_start + l];

			error = print_patch_line(
				patch->delta, &hunk->range,
				line->origin, line->ptr, line->len, &pi);
		}
	}

	git_buf_free(&temp);

	return error;
}

int git_diff_patch_to_str(
	char **string,
	git_diff_patch *patch)
{
	int error;
	git_buf output = GIT_BUF_INIT;

	error = git_diff_patch_print(patch, print_to_buffer_cb, &output);

	/* GIT_EUSER means git_buf_put in print_to_buffer_cb returned -1,
	 * meaning a memory allocation failure, so just map to -1...
	 */
	if (error == GIT_EUSER)
		error = -1;

	*string = git_buf_detach(&output);

	return error;
}

int git_diff__paired_foreach(
	git_diff_list *idx2head,
	git_diff_list *wd2idx,
	int (*cb)(git_diff_delta *i2h, git_diff_delta *w2i, void *payload),
	void *payload)
{
	int cmp;
	git_diff_delta *i2h, *w2i;
	size_t i, j, i_max, j_max;
	int (*strcomp)(const char *, const char *);

	i_max = idx2head ? idx2head->deltas.length : 0;
	j_max = wd2idx   ? wd2idx->deltas.length   : 0;

   /* Get appropriate strcmp function */
   strcomp = idx2head ? idx2head->strcomp : wd2idx ? wd2idx->strcomp : NULL;

   /* Assert both iterators use matching ignore-case. If this function ever
    * supports merging diffs that are not sorted by the same function, then
    * it will need to spool and sort on one of the results before merging
    */
   if (idx2head && wd2idx) {
       assert(idx2head->strcomp == wd2idx->strcomp);
   }

	for (i = 0, j = 0; i < i_max || j < j_max; ) {
		i2h = idx2head ? GIT_VECTOR_GET(&idx2head->deltas,i) : NULL;
		w2i = wd2idx   ? GIT_VECTOR_GET(&wd2idx->deltas,j)   : NULL;

		cmp = !w2i ? -1 : !i2h ? 1 :
			strcomp(i2h->old_file.path, w2i->old_file.path);

		if (cmp < 0) {
			if (cb(i2h, NULL, payload))
				return GIT_EUSER;
			i++;
		} else if (cmp > 0) {
			if (cb(NULL, w2i, payload))
				return GIT_EUSER;
			j++;
		} else {
			if (cb(i2h, w2i, payload))
				return GIT_EUSER;
			i++; j++;
		}
	}

	return 0;
}
