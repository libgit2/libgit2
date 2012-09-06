/*
 * Copyright (C) 2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#include "common.h"
#include "git2/diff.h"
#include "git2/attr.h"
#include "git2/blob.h"
#include "git2/oid.h"
#include "xdiff/xdiff.h"
#include <ctype.h>
#include "diff.h"
#include "map.h"
#include "fileops.h"
#include "filter.h"

/*
 * A diff_delta_context represents all of the information that goes into
 * processing the diff of an observed file change.  In the case of the
 * git_diff_foreach() call it is an emphemeral structure that is filled
 * in to execute each diff.  In the case of a git_diff_iterator, it holds
 * most of the information for the diff in progress.
 */
typedef struct {
	git_repository   *repo;
	git_diff_options *opts;
	xdemitconf_t xdiff_config;
	xpparam_t    xdiff_params;
	git_diff_delta *delta;
	uint32_t prepped  : 1;
	uint32_t loaded   : 1;
	uint32_t diffable : 1;
	uint32_t diffed   : 1;
	git_iterator_type_t old_src;
	git_iterator_type_t new_src;
	git_blob *old_blob;
	git_blob *new_blob;
	git_map   old_data;
	git_map   new_data;
	void *cb_data;
	git_diff_hunk_fn per_hunk;
	git_diff_data_fn per_line;
	int cb_error;
	git_diff_range range;
} diff_delta_context;

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

static int format_hunk_header(char *header, size_t len, git_diff_range *range)
{
	if (range->old_lines != 1) {
		if (range->new_lines != 1)
			return p_snprintf(
				header, len, "@@ -%d,%d +%d,%d @@",
				range->old_start, range->old_lines,
				range->new_start, range->new_lines);
		else
			return p_snprintf(
				header, len, "@@ -%d,%d +%d @@",
				range->old_start, range->old_lines, range->new_start);
	} else {
		if (range->new_lines != 1)
			return p_snprintf(
				header, len, "@@ -%d +%d,%d @@",
				range->old_start, range->new_start, range->new_lines);
		else
			return p_snprintf(
				header, len, "@@ -%d +%d @@",
				range->old_start, range->new_start);
	}
}

static bool diff_delta_is_ambiguous(git_diff_delta *delta)
{
	return (git_oid_iszero(&delta->new_file.oid) &&
			(delta->new_file.flags & GIT_DIFF_FILE_VALID_OID) == 0 &&
			delta->status == GIT_DELTA_MODIFIED);
}

static bool diff_delta_should_skip(git_diff_options *opts, git_diff_delta *delta)
{
	if (delta->status == GIT_DELTA_UNMODIFIED &&
		(opts->flags & GIT_DIFF_INCLUDE_UNMODIFIED) == 0)
		return true;

	if (delta->status == GIT_DELTA_IGNORED &&
		(opts->flags & GIT_DIFF_INCLUDE_IGNORED) == 0)
		return true;

	if (delta->status == GIT_DELTA_UNTRACKED &&
		(opts->flags & GIT_DIFF_INCLUDE_UNTRACKED) == 0)
		return true;

	return false;
}

#define BINARY_DIFF_FLAGS (GIT_DIFF_FILE_BINARY|GIT_DIFF_FILE_NOT_BINARY)

static int update_file_is_binary_by_attr(
	git_repository *repo, git_diff_file *file)
{
	const char *value;

	if (!repo)
		return 0;

	if (git_attr_get(&value, repo, 0, file->path, "diff") < 0)
		return -1;

	if (GIT_ATTR_FALSE(value))
		file->flags |= GIT_DIFF_FILE_BINARY;
	else if (GIT_ATTR_TRUE(value))
		file->flags |= GIT_DIFF_FILE_NOT_BINARY;
	/* otherwise leave file->flags alone */

	return 0;
}

static void update_delta_is_binary(git_diff_delta *delta)
{
	if ((delta->old_file.flags & GIT_DIFF_FILE_BINARY) != 0 ||
		(delta->new_file.flags & GIT_DIFF_FILE_BINARY) != 0)
		delta->binary = 1;
	else if ((delta->old_file.flags & GIT_DIFF_FILE_NOT_BINARY) != 0 ||
			 (delta->new_file.flags & GIT_DIFF_FILE_NOT_BINARY) != 0)
		delta->binary = 0;
	/* otherwise leave delta->binary value untouched */
}

static int diff_delta_is_binary_by_attr(diff_delta_context *ctxt)
{
	int error = 0, mirror_new;
	git_diff_delta *delta = ctxt->delta;

	delta->binary = -1;

	/* make sure files are conceivably mmap-able */
	if ((git_off_t)((size_t)delta->old_file.size) != delta->old_file.size ||
		(git_off_t)((size_t)delta->new_file.size) != delta->new_file.size)
	{
		delta->old_file.flags |= GIT_DIFF_FILE_BINARY;
		delta->new_file.flags |= GIT_DIFF_FILE_BINARY;
		delta->binary = 1;
		return 0;
	}

	/* check if user is forcing us to text diff these files */
	if (ctxt->opts->flags & GIT_DIFF_FORCE_TEXT) {
		delta->old_file.flags |= GIT_DIFF_FILE_NOT_BINARY;
		delta->new_file.flags |= GIT_DIFF_FILE_NOT_BINARY;
		delta->binary = 0;
		return 0;
	}

	/* check diff attribute +, -, or 0 */
	if (update_file_is_binary_by_attr(ctxt->repo, &delta->old_file) < 0)
		return -1;

	mirror_new = (delta->new_file.path == delta->old_file.path ||
				  strcmp(delta->new_file.path, delta->old_file.path) == 0);
	if (mirror_new)
		delta->new_file.flags |= (delta->old_file.flags & BINARY_DIFF_FLAGS);
	else
		error = update_file_is_binary_by_attr(ctxt->repo, &delta->new_file);

	update_delta_is_binary(delta);

	return error;
}

static int diff_delta_is_binary_by_content(diff_delta_context *ctxt)
{
	git_diff_delta *delta = ctxt->delta;
	git_buf search;

	if ((delta->old_file.flags & BINARY_DIFF_FLAGS) == 0) {
		search.ptr  = ctxt->old_data.data;
		search.size = min(ctxt->old_data.len, 4000);

		if (git_buf_is_binary(&search))
			delta->old_file.flags |= GIT_DIFF_FILE_BINARY;
		else
			delta->old_file.flags |= GIT_DIFF_FILE_NOT_BINARY;
	}

	if ((delta->new_file.flags & BINARY_DIFF_FLAGS) == 0) {
		search.ptr  = ctxt->new_data.data;
		search.size = min(ctxt->new_data.len, 4000);

		if (git_buf_is_binary(&search))
			delta->new_file.flags |= GIT_DIFF_FILE_BINARY;
		else
			delta->new_file.flags |= GIT_DIFF_FILE_NOT_BINARY;
	}

	update_delta_is_binary(delta);

	/* TODO: if value != NULL, implement diff drivers */

	return 0;
}

static void setup_xdiff_options(
	git_diff_options *opts, xdemitconf_t *cfg, xpparam_t *param)
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
	git_repository *repo,
	const git_oid *oid,
	git_map *map,
	git_blob **blob)
{
	if (git_oid_iszero(oid))
		return 0;

	if (git_blob_lookup(blob, repo, oid) < 0)
		return -1;

	map->data = (void *)git_blob_rawcontent(*blob);
	map->len  = git_blob_rawsize(*blob);
	return 0;
}

static int get_workdir_content(
	git_repository *repo,
	git_diff_file *file,
	git_map *map)
{
	int error = 0;
	git_buf path = GIT_BUF_INIT;

	if (git_buf_joinpath(&path, git_repository_workdir(repo), file->path) < 0)
		return -1;

	if (S_ISLNK(file->mode)) {
		ssize_t alloc_len, read_len;

		file->flags |= GIT_DIFF_FILE_FREE_DATA;
		file->flags |= GIT_DIFF_FILE_BINARY;

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
		} else
			map->len = read_len;
	}
	else {
		error = git_futils_mmap_ro_file(map, path.ptr);
		file->flags |= GIT_DIFF_FILE_UNMAP_DATA;
	}
	git_buf_free(&path);
	return error;
}

static void release_content(git_diff_file *file, git_map *map, git_blob *blob)
{
	if (blob != NULL)
		git_blob_free(blob);

	if (file->flags & GIT_DIFF_FILE_FREE_DATA) {
		git__free(map->data);
		map->data = "";
		map->len  = 0;
		file->flags &= ~GIT_DIFF_FILE_FREE_DATA;
	}
	else if (file->flags & GIT_DIFF_FILE_UNMAP_DATA) {
		git_futils_mmap_free(map);
		map->data = "";
		map->len  = 0;
		file->flags &= ~GIT_DIFF_FILE_UNMAP_DATA;
	}
}

static void diff_delta_init_context(
	diff_delta_context *ctxt,
	git_repository   *repo,
	git_diff_options *opts,
	git_iterator_type_t old_src,
	git_iterator_type_t new_src)
{
	memset(ctxt, 0, sizeof(diff_delta_context));

	ctxt->repo = repo;
	ctxt->opts = opts;
	ctxt->old_src = old_src;
	ctxt->new_src = new_src;

	setup_xdiff_options(opts, &ctxt->xdiff_config, &ctxt->xdiff_params);
}

static void diff_delta_init_context_from_diff_list(
	diff_delta_context *ctxt,
	git_diff_list *diff)
{
	diff_delta_init_context(
		ctxt, diff->repo, &diff->opts, diff->old_src, diff->new_src);
}

static void diff_delta_unload(diff_delta_context *ctxt)
{
	ctxt->diffed = 0;

	if (ctxt->loaded) {
		release_content(&ctxt->delta->old_file, &ctxt->old_data, ctxt->old_blob);
		release_content(&ctxt->delta->new_file, &ctxt->new_data, ctxt->new_blob);
		ctxt->loaded = 0;
	}

	ctxt->delta = NULL;
	ctxt->prepped = 0;
}

static int diff_delta_prep(diff_delta_context *ctxt)
{
	int error;

	if (ctxt->prepped || !ctxt->delta)
		return 0;

	error = diff_delta_is_binary_by_attr(ctxt);

	ctxt->prepped = !error;

	return error;
}

static int diff_delta_load(diff_delta_context *ctxt)
{
	int error = 0;
	git_diff_delta *delta = ctxt->delta;

	if (ctxt->loaded || !ctxt->delta)
		return 0;

	if (!ctxt->prepped && (error = diff_delta_prep(ctxt)) < 0)
		goto cleanup;

	ctxt->old_data.data = "";
	ctxt->old_data.len  = 0;
	ctxt->old_blob      = NULL;

	if (!error && delta->binary != 1 &&
		(delta->status == GIT_DELTA_DELETED ||
		 delta->status == GIT_DELTA_MODIFIED))
	{
		if (ctxt->old_src == GIT_ITERATOR_WORKDIR)
			error = get_workdir_content(
				ctxt->repo, &delta->old_file, &ctxt->old_data);
		else {
			error = get_blob_content(
				ctxt->repo, &delta->old_file.oid,
				&ctxt->old_data, &ctxt->old_blob);

			if (ctxt->new_src == GIT_ITERATOR_WORKDIR) {
				/* TODO: convert crlf of blob content */
			}
		}
	}

	ctxt->new_data.data = "";
	ctxt->new_data.len  = 0;
	ctxt->new_blob      = NULL;

	if (!error && delta->binary != 1 &&
		(delta->status == GIT_DELTA_ADDED ||
		 delta->status == GIT_DELTA_MODIFIED))
	{
		if (ctxt->new_src == GIT_ITERATOR_WORKDIR)
			error = get_workdir_content(
				ctxt->repo, &delta->new_file, &ctxt->new_data);
		else {
			error = get_blob_content(
				ctxt->repo, &delta->new_file.oid,
				&ctxt->new_data, &ctxt->new_blob);

			if (ctxt->old_src == GIT_ITERATOR_WORKDIR) {
				/* TODO: convert crlf of blob content */
			}
		}

		if (!error && !(delta->new_file.flags & GIT_DIFF_FILE_VALID_OID)) {
			error = git_odb_hash(
				&delta->new_file.oid, ctxt->new_data.data,
				ctxt->new_data.len, GIT_OBJ_BLOB);
			if (error < 0)
				goto cleanup;

			delta->new_file.flags |= GIT_DIFF_FILE_VALID_OID;

			/* since we did not have the definitive oid, we may have
			 * incorrect status and need to skip this item.
			 */
			if (delta->old_file.mode == delta->new_file.mode &&
				!git_oid_cmp(&delta->old_file.oid, &delta->new_file.oid))
			{
				delta->status = GIT_DELTA_UNMODIFIED;

				if ((ctxt->opts->flags & GIT_DIFF_INCLUDE_UNMODIFIED) == 0)
					goto cleanup;
			}
		}
	}

	/* if we have not already decided whether file is binary,
	 * check the first 4K for nul bytes to decide...
	 */
	if (!error && delta->binary == -1)
		error = diff_delta_is_binary_by_content(ctxt);

cleanup:
	ctxt->loaded = !error;

	/* flag if we would want to diff the contents of these files */
	if (ctxt->loaded)
		ctxt->diffable =
			(delta->binary != 1 &&
			 delta->status != GIT_DELTA_UNMODIFIED &&
			 (ctxt->old_data.len || ctxt->new_data.len) &&
			 git_oid_cmp(&delta->old_file.oid, &delta->new_file.oid));

	return error;
}

static int diff_delta_cb(void *priv, mmbuffer_t *bufs, int len)
{
	diff_delta_context *ctxt = priv;

	if (len == 1) {
		if ((ctxt->cb_error = parse_hunk_header(&ctxt->range, bufs[0].ptr)) < 0)
			return ctxt->cb_error;

		if (ctxt->per_hunk != NULL &&
			ctxt->per_hunk(ctxt->cb_data, ctxt->delta, &ctxt->range,
				bufs[0].ptr, bufs[0].size))
			ctxt->cb_error = GIT_EUSER;
	}

	if (len == 2 || len == 3) {
		/* expect " "/"-"/"+", then data */
		char origin =
			(*bufs[0].ptr == '+') ? GIT_DIFF_LINE_ADDITION :
			(*bufs[0].ptr == '-') ? GIT_DIFF_LINE_DELETION :
			GIT_DIFF_LINE_CONTEXT;

		if (ctxt->per_line != NULL &&
			ctxt->per_line(ctxt->cb_data, ctxt->delta, &ctxt->range, origin,
				bufs[1].ptr, bufs[1].size))
			ctxt->cb_error = GIT_EUSER;
	}

	if (len == 3 && !ctxt->cb_error) {
		/* This should only happen if we are adding a line that does not
		 * have a newline at the end and the old code did.  In that case,
		 * we have a ADD with a DEL_EOFNL as a pair.
		 */
		char origin =
			(*bufs[0].ptr == '+') ? GIT_DIFF_LINE_DEL_EOFNL :
			(*bufs[0].ptr == '-') ? GIT_DIFF_LINE_ADD_EOFNL :
			GIT_DIFF_LINE_CONTEXT;

		if (ctxt->per_line != NULL &&
			ctxt->per_line(ctxt->cb_data, ctxt->delta, &ctxt->range, origin,
				bufs[2].ptr, bufs[2].size))
			ctxt->cb_error = GIT_EUSER;
	}

	return ctxt->cb_error;
}

static int diff_delta_exec(
	diff_delta_context *ctxt,
	void *cb_data,
	git_diff_hunk_fn per_hunk,
	git_diff_data_fn per_line)
{
	int error = 0;
	xdemitcb_t xdiff_callback;
	mmfile_t old_xdiff_data, new_xdiff_data;

	if (ctxt->diffed || !ctxt->delta)
		return 0;

	if (!ctxt->loaded && (error = diff_delta_load(ctxt)) < 0)
		goto cleanup;

	if (!ctxt->diffable)
		return 0;

	ctxt->cb_data  = cb_data;
	ctxt->per_hunk = per_hunk;
	ctxt->per_line = per_line;
	ctxt->cb_error = 0;

	memset(&xdiff_callback, 0, sizeof(xdiff_callback));
	xdiff_callback.outf = diff_delta_cb;
	xdiff_callback.priv = ctxt;

	old_xdiff_data.ptr  = ctxt->old_data.data;
	old_xdiff_data.size = ctxt->old_data.len;
	new_xdiff_data.ptr  = ctxt->new_data.data;
	new_xdiff_data.size = ctxt->new_data.len;

	xdl_diff(&old_xdiff_data, &new_xdiff_data,
		&ctxt->xdiff_params, &ctxt->xdiff_config, &xdiff_callback);

	error = ctxt->cb_error;

cleanup:
	ctxt->diffed = !error;

	return error;
}

int git_diff_foreach(
	git_diff_list *diff,
	void *data,
	git_diff_file_fn file_cb,
	git_diff_hunk_fn hunk_cb,
	git_diff_data_fn line_cb)
{
	int error = 0;
	diff_delta_context ctxt;
	size_t idx;

	diff_delta_init_context_from_diff_list(&ctxt, diff);

	git_vector_foreach(&diff->deltas, idx, ctxt.delta) {
		if (diff_delta_is_ambiguous(ctxt.delta))
			if ((error = diff_delta_load(&ctxt)) < 0)
				goto cleanup;

		if (diff_delta_should_skip(ctxt.opts, ctxt.delta))
			continue;

		if ((error = diff_delta_load(&ctxt)) < 0)
			goto cleanup;

		if (file_cb != NULL &&
			file_cb(data, ctxt.delta, (float)idx / diff->deltas.length) != 0)
		{
			error = GIT_EUSER;
			goto cleanup;
		}

		error = diff_delta_exec(&ctxt, data, hunk_cb, line_cb);

cleanup:
		diff_delta_unload(&ctxt);

		if (error < 0)
			break;
	}

	if (error == GIT_EUSER)
		giterr_clear();

	return error;
}

typedef struct {
	git_diff_list *diff;
	git_diff_data_fn print_cb;
	void *cb_data;
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

static int print_compact(void *data, git_diff_delta *delta, float progress)
{
	diff_print_info *pi = data;
	char code, old_suffix, new_suffix;

	GIT_UNUSED(progress);

	switch (delta->status) {
	case GIT_DELTA_ADDED: code = 'A'; break;
	case GIT_DELTA_DELETED: code = 'D'; break;
	case GIT_DELTA_MODIFIED: code = 'M'; break;
	case GIT_DELTA_RENAMED: code = 'R'; break;
	case GIT_DELTA_COPIED: code = 'C'; break;
	case GIT_DELTA_IGNORED: code = 'I'; break;
	case GIT_DELTA_UNTRACKED: code = '?'; break;
	default: code = 0;
	}

	if (!code)
		return 0;

	old_suffix = pick_suffix(delta->old_file.mode);
	new_suffix = pick_suffix(delta->new_file.mode);

	git_buf_clear(pi->buf);

	if (delta->old_file.path != delta->new_file.path &&
		strcmp(delta->old_file.path,delta->new_file.path) != 0)
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

	if (pi->print_cb(pi->cb_data, delta, NULL, GIT_DIFF_LINE_FILE_HDR,
			git_buf_cstr(pi->buf), git_buf_len(pi->buf)))
	{
		giterr_clear();
		return GIT_EUSER;
	}

	return 0;
}

int git_diff_print_compact(
	git_diff_list *diff,
	void *cb_data,
	git_diff_data_fn print_cb)
{
	int error;
	git_buf buf = GIT_BUF_INIT;
	diff_print_info pi;

	pi.diff     = diff;
	pi.print_cb = print_cb;
	pi.cb_data  = cb_data;
	pi.buf      = &buf;

	error = git_diff_foreach(diff, &pi, print_compact, NULL, NULL);

	git_buf_free(&buf);

	return error;
}


static int print_oid_range(diff_print_info *pi, git_diff_delta *delta)
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

static int print_patch_file(void *data, git_diff_delta *delta, float progress)
{
	diff_print_info *pi = data;
	const char *oldpfx = pi->diff->opts.old_prefix;
	const char *oldpath = delta->old_file.path;
	const char *newpfx = pi->diff->opts.new_prefix;
	const char *newpath = delta->new_file.path;

	GIT_UNUSED(progress);

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

	if (delta->binary != 1) {
		git_buf_printf(pi->buf, "--- %s%s\n", oldpfx, oldpath);
		git_buf_printf(pi->buf, "+++ %s%s\n", newpfx, newpath);
	}

	if (git_buf_oom(pi->buf))
		return -1;

    if (pi->print_cb(pi->cb_data, delta, NULL, GIT_DIFF_LINE_FILE_HDR, git_buf_cstr(pi->buf), git_buf_len(pi->buf)))
	{
		giterr_clear();
		return GIT_EUSER;
	}

    if (delta->binary != 1)
        return 0;

	git_buf_clear(pi->buf);
	git_buf_printf(
		pi->buf, "Binary files %s%s and %s%s differ\n",
		oldpfx, oldpath, newpfx, newpath);
	if (git_buf_oom(pi->buf))
		return -1;

	if (pi->print_cb(pi->cb_data, delta, NULL, GIT_DIFF_LINE_BINARY,
			git_buf_cstr(pi->buf), git_buf_len(pi->buf)))
	{
		giterr_clear();
		return GIT_EUSER;
	}

	return 0;
}

static int print_patch_hunk(
	void *data,
	git_diff_delta *d,
	git_diff_range *r,
	const char *header,
	size_t header_len)
{
	diff_print_info *pi = data;

	git_buf_clear(pi->buf);
	if (git_buf_printf(pi->buf, "%.*s", (int)header_len, header) < 0)
		return -1;

	if (pi->print_cb(pi->cb_data, d, r, GIT_DIFF_LINE_HUNK_HDR,
			git_buf_cstr(pi->buf), git_buf_len(pi->buf)))
	{
		giterr_clear();
		return GIT_EUSER;
	}

	return 0;
}

static int print_patch_line(
	void *data,
	git_diff_delta *delta,
	git_diff_range *range,
	char line_origin, /* GIT_DIFF_LINE value from above */
	const char *content,
	size_t content_len)
{
	diff_print_info *pi = data;

	git_buf_clear(pi->buf);

	if (line_origin == GIT_DIFF_LINE_ADDITION ||
		line_origin == GIT_DIFF_LINE_DELETION ||
		line_origin == GIT_DIFF_LINE_CONTEXT)
		git_buf_printf(pi->buf, "%c%.*s", line_origin, (int)content_len, content);
	else if (content_len > 0)
		git_buf_printf(pi->buf, "%.*s", (int)content_len, content);

	if (git_buf_oom(pi->buf))
		return -1;

	if (pi->print_cb(pi->cb_data, delta, range, line_origin,
			git_buf_cstr(pi->buf), git_buf_len(pi->buf)))
	{
		giterr_clear();
		return GIT_EUSER;
	}

	return 0;
}

int git_diff_print_patch(
	git_diff_list *diff,
	void *cb_data,
	git_diff_data_fn print_cb)
{
	int error;
	git_buf buf = GIT_BUF_INIT;
	diff_print_info pi;

	pi.diff     = diff;
	pi.print_cb = print_cb;
	pi.cb_data  = cb_data;
	pi.buf      = &buf;

	error = git_diff_foreach(
		diff, &pi, print_patch_file, print_patch_hunk, print_patch_line);

	git_buf_free(&buf);

	return error;
}

int git_diff_entrycount(git_diff_list *diff, int delta_t)
{
	int count = 0;
	unsigned int i;
	git_diff_delta *delta;

	assert(diff);

	git_vector_foreach(&diff->deltas, i, delta) {
		if (diff_delta_should_skip(&diff->opts, delta))
			continue;

		if (delta_t < 0 || delta->status == (git_delta_t)delta_t)
			count++;
	}

	/* It is possible that this has overcounted the number of diffs because
	 * there may be entries that are marked as MODIFIED due to differences
	 * in stat() output that will turn out to be the same once we calculate
	 * the actual SHA of the data on disk.
	 */

	return count;
}

static void set_data_from_blob(
	git_blob *blob, git_map *map, git_diff_file *file)
{
	if (blob) {
		map->data = (char *)git_blob_rawcontent(blob);
		file->size = map->len = git_blob_rawsize(blob);
		git_oid_cpy(&file->oid, git_object_id((const git_object *)blob));
	} else {
		map->data = "";
		file->size = map->len = 0;
	}
}

int git_diff_blobs(
	git_blob *old_blob,
	git_blob *new_blob,
	git_diff_options *options,
	void *cb_data,
	git_diff_file_fn file_cb,
	git_diff_hunk_fn hunk_cb,
	git_diff_data_fn line_cb)
{
	int error;
	diff_delta_context ctxt;
	git_diff_delta delta;
	git_blob *new, *old;

	new = new_blob;
	old = old_blob;

	if (options && (options->flags & GIT_DIFF_REVERSE)) {
		git_blob *swap = old;
		old = new;
		new = swap;
	}

	diff_delta_init_context(
		&ctxt, NULL, options, GIT_ITERATOR_TREE, GIT_ITERATOR_TREE);

	/* populate a "fake" delta record */

	memset(&delta, 0, sizeof(delta));

	set_data_from_blob(old, &ctxt.old_data, &delta.old_file);
	set_data_from_blob(new, &ctxt.new_data, &delta.new_file);

	delta.status = new ?
		(old ? GIT_DELTA_MODIFIED : GIT_DELTA_ADDED) :
		(old ? GIT_DELTA_DELETED : GIT_DELTA_UNTRACKED);

	if (git_oid_cmp(&delta.new_file.oid, &delta.old_file.oid) == 0)
		delta.status = GIT_DELTA_UNMODIFIED;

	ctxt.delta = &delta;

	if ((error = diff_delta_prep(&ctxt)) < 0)
		goto cleanup;

	if (delta.binary == -1 &&
		(error = diff_delta_is_binary_by_content(&ctxt)) < 0)
		goto cleanup;

	ctxt.loaded = 1;
	ctxt.diffable = (delta.binary != 1 && delta.status != GIT_DELTA_UNMODIFIED);

	/* do diffs */

	if (file_cb != NULL && file_cb(cb_data, &delta, 1)) {
		error = GIT_EUSER;
		goto cleanup;
	}

	error = diff_delta_exec(&ctxt, cb_data, hunk_cb, line_cb);

cleanup:
	if (error == GIT_EUSER)
		giterr_clear();

	diff_delta_unload(&ctxt);

	return error;
}

typedef struct diffiter_line diffiter_line;
struct diffiter_line {
	diffiter_line *next;
	char origin;
	const char *ptr;
	size_t len;
};

typedef struct diffiter_hunk diffiter_hunk;
struct diffiter_hunk {
	diffiter_hunk *next;
	git_diff_range range;
	diffiter_line *line_head;
	size_t line_count;
};

struct git_diff_iterator {
	git_diff_list *diff;
	diff_delta_context ctxt;
	size_t file_index;
	size_t next_index;
	size_t file_count;
	git_pool hunks;
	size_t   hunk_count;
	diffiter_hunk *hunk_head;
	diffiter_hunk *hunk_curr;
	char hunk_header[128];
	git_pool lines;
	diffiter_line *line_curr;
};

typedef struct {
	git_diff_iterator *iter;
	diffiter_hunk *last_hunk;
	diffiter_line *last_line;
} diffiter_cb_info;

static int diffiter_hunk_cb(
	void *cb_data,
	git_diff_delta *delta,
	git_diff_range *range,
	const char *header,
	size_t header_len)
{
	diffiter_cb_info *info = cb_data;
	git_diff_iterator *iter = info->iter;
	diffiter_hunk *hunk;

	GIT_UNUSED(delta);
	GIT_UNUSED(header);
	GIT_UNUSED(header_len);

	if ((hunk = git_pool_mallocz(&iter->hunks, 1)) == NULL) {
		iter->ctxt.cb_error = -1;
		return -1;
	}

	if (info->last_hunk)
		info->last_hunk->next = hunk;
	info->last_hunk = hunk;

	memcpy(&hunk->range, range, sizeof(hunk->range));

	iter->hunk_count++;

	if (iter->hunk_head == NULL)
		iter->hunk_curr = iter->hunk_head = hunk;

	return 0;
}

static int diffiter_line_cb(
	void *cb_data,
	git_diff_delta *delta,
	git_diff_range *range,
	char line_origin,
	const char *content,
	size_t content_len)
{
	diffiter_cb_info *info = cb_data;
	git_diff_iterator *iter = info->iter;
	diffiter_line *line;

	GIT_UNUSED(delta);
	GIT_UNUSED(range);

	if ((line = git_pool_mallocz(&iter->lines, 1)) == NULL) {
		iter->ctxt.cb_error = -1;
		return -1;
	}

	if (info->last_line)
		info->last_line->next = line;
	info->last_line = line;

	line->origin = line_origin;
	line->ptr = content;
	line->len = content_len;

	info->last_hunk->line_count++;

	if (info->last_hunk->line_head == NULL)
		info->last_hunk->line_head = line;

	return 0;
}

static int diffiter_do_diff_file(git_diff_iterator *iter)
{
	int error;
	diffiter_cb_info info;

	if (iter->ctxt.diffed || !iter->ctxt.delta)
		return 0;

	memset(&info, 0, sizeof(info));
	info.iter = iter;

	error = diff_delta_exec(
		&iter->ctxt, &info, diffiter_hunk_cb, diffiter_line_cb);

	if (error == GIT_EUSER)
		error = iter->ctxt.cb_error;

	return error;
}

static void diffiter_do_unload_file(git_diff_iterator *iter)
{
	if (iter->ctxt.loaded) {
		diff_delta_unload(&iter->ctxt);

		git_pool_clear(&iter->lines);
		git_pool_clear(&iter->hunks);
	}

	iter->ctxt.delta = NULL;
	iter->hunk_head = NULL;
	iter->hunk_count = 0;
}

int git_diff_iterator_new(
	git_diff_iterator **iterator_ptr,
	git_diff_list *diff)
{
	size_t i;
	git_diff_delta *delta;
	git_diff_iterator *iter;

	assert(diff && iterator_ptr);

	*iterator_ptr = NULL;

	iter = git__malloc(sizeof(git_diff_iterator));
	GITERR_CHECK_ALLOC(iter);

	memset(iter, 0, sizeof(*iter));

	iter->diff = diff;
	GIT_REFCOUNT_INC(iter->diff);

	diff_delta_init_context_from_diff_list(&iter->ctxt, diff);

	if (git_pool_init(&iter->hunks, sizeof(diffiter_hunk), 0) < 0 ||
		git_pool_init(&iter->lines, sizeof(diffiter_line), 0) < 0)
		goto fail;

	git_vector_foreach(&diff->deltas, i, delta) {
		if (diff_delta_should_skip(iter->ctxt.opts, delta))
			continue;
		iter->file_count++;
	}

	*iterator_ptr = iter;

	return 0;

fail:
	git_diff_iterator_free(iter);

	return -1;
}

void git_diff_iterator_free(git_diff_iterator *iter)
{
	diffiter_do_unload_file(iter);
	git_diff_list_free(iter->diff); /* decrement ref count */
	git__free(iter);
}

int git_diff_iterator_num_files(git_diff_iterator *iter)
{
	return (int)iter->file_count;
}

int git_diff_iterator_num_hunks_in_file(git_diff_iterator *iter)
{
	int error = diffiter_do_diff_file(iter);
	return (error != 0) ? error : (int)iter->hunk_count;
}

int git_diff_iterator_num_lines_in_hunk(git_diff_iterator *iter)
{
	int error = diffiter_do_diff_file(iter);
	if (!error && iter->hunk_curr)
		error = iter->hunk_curr->line_count;
	return error;
}

int git_diff_iterator_next_file(
	git_diff_delta **delta_ptr,
	git_diff_iterator *iter)
{
	int error = 0;

	assert(iter);

	iter->file_index = iter->next_index;

	diffiter_do_unload_file(iter);

	while (!error) {
		iter->ctxt.delta = git_vector_get(&iter->diff->deltas, iter->file_index);
		if (!iter->ctxt.delta) {
			error = GIT_ITEROVER;
			break;
		}

		if (diff_delta_is_ambiguous(iter->ctxt.delta) &&
			(error = diff_delta_load(&iter->ctxt)) < 0)
			break;

		if (!diff_delta_should_skip(iter->ctxt.opts, iter->ctxt.delta))
			break;

		iter->file_index++;
	}

	if (!error) {
		iter->next_index = iter->file_index + 1;

		error = diff_delta_prep(&iter->ctxt);
	}

	if (iter->ctxt.delta == NULL) {
		iter->hunk_curr = NULL;
		iter->line_curr = NULL;
	}

	if (delta_ptr != NULL)
		*delta_ptr = !error ? iter->ctxt.delta : NULL;

	return error;
}

int git_diff_iterator_next_hunk(
	git_diff_range **range_ptr,
	const char **header,
	size_t *header_len,
	git_diff_iterator *iter)
{
	int error = diffiter_do_diff_file(iter);
	git_diff_range *range;

	if (error)
		return error;

	if (iter->hunk_curr == NULL) {
		if (range_ptr) *range_ptr = NULL;
		if (header) *header = NULL;
		if (header_len) *header_len = 0;
		iter->line_curr = NULL;
		return GIT_ITEROVER;
	}

	range = &iter->hunk_curr->range;

	if (range_ptr)
		*range_ptr = range;

	if (header) {
		int out = format_hunk_header(
			iter->hunk_header, sizeof(iter->hunk_header), range);

		/* TODO: append function name to header */

		*(iter->hunk_header + out++) = '\n';

		*header = iter->hunk_header;

		if (header_len)
			*header_len = (size_t)out;
	}

	iter->line_curr = iter->hunk_curr->line_head;
	iter->hunk_curr = iter->hunk_curr->next;

	return error;
}

int git_diff_iterator_next_line(
	char *line_origin, /**< GIT_DIFF_LINE_... value from above */
	const char **content_ptr,
	size_t *content_len,
	git_diff_iterator *iter)
{
	int error = diffiter_do_diff_file(iter);

	if (error)
		return error;

	/* if the user has not called next_hunk yet, call it implicitly (OK?) */
	if (iter->hunk_curr == iter->hunk_head) {
		error = git_diff_iterator_next_hunk(NULL, NULL, NULL, iter);
		if (error)
			return error;
	}

	if (iter->line_curr == NULL) {
		if (line_origin) *line_origin = GIT_DIFF_LINE_CONTEXT;
		if (content_ptr) *content_ptr = NULL;
		if (content_len) *content_len = 0;
		return GIT_ITEROVER;
	}

	if (line_origin)
		*line_origin = iter->line_curr->origin;
	if (content_ptr)
		*content_ptr = iter->line_curr->ptr;
	if (content_len)
		*content_len = iter->line_curr->len;

	iter->line_curr = iter->line_curr->next;

	return error;
}
