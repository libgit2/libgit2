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
#include "xdiff/xdiff.h"
#include <ctype.h>
#include "diff.h"
#include "map.h"
#include "fileops.h"
#include "filter.h"

typedef struct {
	git_diff_list *diff;
	void *cb_data;
	git_diff_hunk_fn hunk_cb;
	git_diff_data_fn line_cb;
	unsigned int index;
	git_diff_delta *delta;
	git_diff_range range;
} diff_output_info;

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

static int diff_output_cb(void *priv, mmbuffer_t *bufs, int len)
{
	diff_output_info *info = priv;

	if (len == 1 && info->hunk_cb) {
		git_diff_range range = { -1, 0, -1, 0 };
		const char *scan = bufs[0].ptr;

		/* expect something of the form "@@ -%d[,%d] +%d[,%d] @@" */
		if (*scan != '@')
			return -1;

		if (read_next_int(&scan, &range.old_start) < 0)
			return -1;
		if (*scan == ',' && read_next_int(&scan, &range.old_lines) < 0)
			return -1;

		if (read_next_int(&scan, &range.new_start) < 0)
			return -1;
		if (*scan == ',' && read_next_int(&scan, &range.new_lines) < 0)
			return -1;

		if (range.old_start < 0 || range.new_start < 0)
			return -1;

		memcpy(&info->range, &range, sizeof(git_diff_range));

		return info->hunk_cb(
			info->cb_data, info->delta, &range, bufs[0].ptr, bufs[0].size);
	}

	if ((len == 2 || len == 3) && info->line_cb) {
		int origin;

		/* expect " "/"-"/"+", then data, then maybe newline */
		origin =
			(*bufs[0].ptr == '+') ? GIT_DIFF_LINE_ADDITION :
			(*bufs[0].ptr == '-') ? GIT_DIFF_LINE_DELETION :
			GIT_DIFF_LINE_CONTEXT;

		if (info->line_cb(
			info->cb_data, info->delta, &info->range, origin, bufs[1].ptr, bufs[1].size) < 0)
			return -1;

		/* deal with adding and removing newline at EOF */
		if (len == 3) {
			if (origin == GIT_DIFF_LINE_ADDITION)
				origin = GIT_DIFF_LINE_ADD_EOFNL;
			else
				origin = GIT_DIFF_LINE_DEL_EOFNL;

			return info->line_cb(
				info->cb_data, info->delta, &info->range, origin, bufs[2].ptr, bufs[2].size);
		}
	}

	return 0;
}

#define BINARY_DIFF_FLAGS (GIT_DIFF_FILE_BINARY|GIT_DIFF_FILE_NOT_BINARY)

static int update_file_is_binary_by_attr(git_repository *repo, git_diff_file *file)
{
	const char *value;
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

static int file_is_binary_by_attr(
	git_diff_list *diff,
	git_diff_delta *delta)
{
	int error = 0, mirror_new;

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
	if (diff->opts.flags & GIT_DIFF_FORCE_TEXT) {
		delta->old_file.flags |= GIT_DIFF_FILE_NOT_BINARY;
		delta->new_file.flags |= GIT_DIFF_FILE_NOT_BINARY;
		delta->binary = 0;
		return 0;
	}

	/* check diff attribute +, -, or 0 */
	if (update_file_is_binary_by_attr(diff->repo, &delta->old_file) < 0)
		return -1;

	mirror_new = (delta->new_file.path == delta->old_file.path ||
				  strcmp(delta->new_file.path, delta->old_file.path) == 0);
	if (mirror_new)
		delta->new_file.flags &= (delta->old_file.flags & BINARY_DIFF_FLAGS);
	else
		error = update_file_is_binary_by_attr(diff->repo, &delta->new_file);

	update_delta_is_binary(delta);

	return error;
}

static int file_is_binary_by_content(
	git_diff_delta *delta,
	git_map *old_data,
	git_map *new_data)
{
	git_buf search;

	if ((delta->old_file.flags & BINARY_DIFF_FLAGS) == 0) {
		search.ptr  = old_data->data;
		search.size = min(old_data->len, 4000);

		if (git_buf_is_binary(&search))
			delta->old_file.flags |= GIT_DIFF_FILE_BINARY;
		else
			delta->old_file.flags |= GIT_DIFF_FILE_NOT_BINARY;
	}

	if ((delta->new_file.flags & BINARY_DIFF_FLAGS) == 0) {
		search.ptr  = new_data->data;
		search.size = min(new_data->len, 4000);

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
		(!opts || !opts->interhunk_lines) ? 3 : opts->interhunk_lines;

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
		ssize_t read_len;

		file->flags |= GIT_DIFF_FILE_FREE_DATA;
		file->flags |= GIT_DIFF_FILE_BINARY;

		map->data = git__malloc((size_t)file->size + 1);
		GITERR_CHECK_ALLOC(map->data);

		read_len = p_readlink(path.ptr, map->data, (size_t)file->size + 1);
		if (read_len != (ssize_t)file->size) {
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
		map->data = NULL;
		file->flags &= ~GIT_DIFF_FILE_FREE_DATA;
	}
	else if (file->flags & GIT_DIFF_FILE_UNMAP_DATA) {
		git_futils_mmap_free(map);
		map->data = NULL;
		file->flags &= ~GIT_DIFF_FILE_UNMAP_DATA;
	}
}

static void fill_map_from_mmfile(git_map *dst, mmfile_t *src) {
	assert(dst && src);

	dst->data = src->ptr;
	dst->len = src->size;
#ifdef GIT_WIN32
	dst->fmh = NULL;
#endif
}

int git_diff_foreach(
	git_diff_list *diff,
	void *data,
	git_diff_file_fn file_cb,
	git_diff_hunk_fn hunk_cb,
	git_diff_data_fn line_cb)
{
	int error = 0;
	diff_output_info info;
	git_diff_delta *delta;
	xpparam_t    xdiff_params;
	xdemitconf_t xdiff_config;
	xdemitcb_t   xdiff_callback;

	info.diff    = diff;
	info.cb_data = data;
	info.hunk_cb = hunk_cb;
	info.line_cb = line_cb;

	setup_xdiff_options(&diff->opts, &xdiff_config, &xdiff_params);
	memset(&xdiff_callback, 0, sizeof(xdiff_callback));
	xdiff_callback.outf = diff_output_cb;
	xdiff_callback.priv = &info;

	git_vector_foreach(&diff->deltas, info.index, delta) {
		git_blob *old_blob = NULL, *new_blob = NULL;
		git_map old_data, new_data;
		mmfile_t old_xdiff_data, new_xdiff_data;

		if (delta->status == GIT_DELTA_UNMODIFIED &&
			(diff->opts.flags & GIT_DIFF_INCLUDE_UNMODIFIED) == 0)
			continue;

		if (delta->status == GIT_DELTA_IGNORED &&
			(diff->opts.flags & GIT_DIFF_INCLUDE_IGNORED) == 0)
			continue;

		if (delta->status == GIT_DELTA_UNTRACKED &&
			(diff->opts.flags & GIT_DIFF_INCLUDE_UNTRACKED) == 0)
			continue;

		if ((error = file_is_binary_by_attr(diff, delta)) < 0)
			goto cleanup;

		old_data.data = "";
		old_data.len = 0;
		new_data.data = "";
		new_data.len  = 0;

		/* TODO: Partial blob reading to defer loading whole blob.
		 * I.e. I want a blob with just the first 4kb loaded, then
		 * later on I will read the rest of the blob if needed.
		 */

		/* map files */
		if (delta->binary != 1 &&
			(hunk_cb || line_cb) &&
			(delta->status == GIT_DELTA_DELETED ||
			 delta->status == GIT_DELTA_MODIFIED))
		{
			if (diff->old_src == GIT_ITERATOR_WORKDIR)
				error = get_workdir_content(diff->repo, &delta->old_file, &old_data);
			else
				error = get_blob_content(
					diff->repo, &delta->old_file.oid, &old_data, &old_blob);

			if (error < 0)
				goto cleanup;
		}

		if (delta->binary != 1 &&
			(hunk_cb || line_cb || git_oid_iszero(&delta->new_file.oid)) &&
			(delta->status == GIT_DELTA_ADDED ||
			 delta->status == GIT_DELTA_MODIFIED))
		{
			if (diff->new_src == GIT_ITERATOR_WORKDIR)
				error = get_workdir_content(diff->repo, &delta->new_file, &new_data);
			else
				error = get_blob_content(
					diff->repo, &delta->new_file.oid, &new_data, &new_blob);

			if (error < 0)
				goto cleanup;

			if ((delta->new_file.flags | GIT_DIFF_FILE_VALID_OID) == 0) {
				error = git_odb_hash(
					&delta->new_file.oid, new_data.data, new_data.len, GIT_OBJ_BLOB);

				if (error < 0)
					goto cleanup;

				/* since we did not have the definitive oid, we may have
				 * incorrect status and need to skip this item.
				 */
				if (git_oid_cmp(&delta->old_file.oid, &delta->new_file.oid) == 0) {
					delta->status = GIT_DELTA_UNMODIFIED;
					if ((diff->opts.flags & GIT_DIFF_INCLUDE_UNMODIFIED) == 0)
						goto cleanup;
				}
			}
		}

		/* if we have not already decided whether file is binary,
		 * check the first 4K for nul bytes to decide...
		 */
		if (delta->binary == -1) {
			error = file_is_binary_by_content(
				delta, &old_data, &new_data);
			if (error < 0)
				goto cleanup;
		}

		/* TODO: if ignore_whitespace is set, then we *must* do text
		 * diffs to tell if a file has really been changed.
		 */

		if (file_cb != NULL) {
			error = file_cb(data, delta, (float)info.index / diff->deltas.length);
			if (error < 0)
				goto cleanup;
		}

		/* don't do hunk and line diffs if file is binary */
		if (delta->binary == 1)
			goto cleanup;

		/* nothing to do if we did not get data */
		if (!old_data.len && !new_data.len)
			goto cleanup;

		assert(hunk_cb || line_cb);

		info.delta = delta;
		old_xdiff_data.ptr = old_data.data;
		old_xdiff_data.size = old_data.len;
		new_xdiff_data.ptr = new_data.data;
		new_xdiff_data.size = new_data.len;

		xdl_diff(&old_xdiff_data, &new_xdiff_data,
			&xdiff_params, &xdiff_config, &xdiff_callback);

cleanup:
		release_content(&delta->old_file, &old_data, old_blob);
		release_content(&delta->new_file, &new_data, new_blob);

		if (error < 0)
			break;
	}

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
	else if (mode & 0100)
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

	return pi->print_cb(pi->cb_data, delta, NULL, GIT_DIFF_LINE_FILE_HDR, git_buf_cstr(pi->buf), git_buf_len(pi->buf));
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
	int result;

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

    result = pi->print_cb(pi->cb_data, delta, NULL, GIT_DIFF_LINE_FILE_HDR, git_buf_cstr(pi->buf), git_buf_len(pi->buf));
    if (result < 0)
        return result;

    if (delta->binary != 1)
        return 0;

	git_buf_clear(pi->buf);
	git_buf_printf(
		pi->buf, "Binary files %s%s and %s%s differ\n",
		oldpfx, oldpath, newpfx, newpath);
	if (git_buf_oom(pi->buf))
		return -1;

	return pi->print_cb(pi->cb_data, delta, NULL, GIT_DIFF_LINE_BINARY, git_buf_cstr(pi->buf), git_buf_len(pi->buf));
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

	return pi->print_cb(pi->cb_data, d, r, GIT_DIFF_LINE_HUNK_HDR, git_buf_cstr(pi->buf), git_buf_len(pi->buf));
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

	return pi->print_cb(pi->cb_data, delta, range, line_origin, git_buf_cstr(pi->buf), git_buf_len(pi->buf));
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

int git_diff_blobs(
	git_blob *old_blob,
	git_blob *new_blob,
	git_diff_options *options,
	void *cb_data,
	git_diff_file_fn file_cb,
	git_diff_hunk_fn hunk_cb,
	git_diff_data_fn line_cb)
{
	diff_output_info info;
	git_diff_delta delta;
	mmfile_t old_data, new_data;
	git_map old_map, new_map;
	xpparam_t xdiff_params;
	xdemitconf_t xdiff_config;
	xdemitcb_t xdiff_callback;
	git_blob *new, *old;

	memset(&delta, 0, sizeof(delta));

	new = new_blob;
	old = old_blob;

	if (options && (options->flags & GIT_DIFF_REVERSE)) {
		git_blob *swap = old;
		old = new;
		new = swap;
	}

	if (old) {
		old_data.ptr  = (char *)git_blob_rawcontent(old);
		old_data.size = git_blob_rawsize(old);
		git_oid_cpy(&delta.old_file.oid, git_object_id((const git_object *)old));
	} else {
		old_data.ptr  = "";
		old_data.size = 0;
	}

	if (new) {
		new_data.ptr  = (char *)git_blob_rawcontent(new);
		new_data.size = git_blob_rawsize(new);
		git_oid_cpy(&delta.new_file.oid, git_object_id((const git_object *)new));
	} else {
		new_data.ptr  = "";
		new_data.size = 0;
	}

	/* populate a "fake" delta record */
	delta.status = new ?
		(old ? GIT_DELTA_MODIFIED : GIT_DELTA_ADDED) :
		(old ? GIT_DELTA_DELETED : GIT_DELTA_UNTRACKED);

	if (git_oid_cmp(&delta.new_file.oid, &delta.old_file.oid) == 0)
		delta.status = GIT_DELTA_UNMODIFIED;

	delta.old_file.size = old_data.size;
	delta.new_file.size = new_data.size;

	fill_map_from_mmfile(&old_map, &old_data);
	fill_map_from_mmfile(&new_map, &new_data);

	if (file_is_binary_by_content(&delta, &old_map, &new_map) < 0)
		return -1;

	if (file_cb != NULL) {
		int error = file_cb(cb_data, &delta, 1);
		if (error < 0)
			return error;
	}

	/* don't do hunk and line diffs if the two blobs are identical */
	if (delta.status == GIT_DELTA_UNMODIFIED)
		return 0;

	/* don't do hunk and line diffs if file is binary */
	if (delta.binary == 1)
		return 0;

	info.diff    = NULL;
	info.delta   = &delta;
	info.cb_data = cb_data;
	info.hunk_cb = hunk_cb;
	info.line_cb = line_cb;

	setup_xdiff_options(options, &xdiff_config, &xdiff_params);
	memset(&xdiff_callback, 0, sizeof(xdiff_callback));
	xdiff_callback.outf = diff_output_cb;
	xdiff_callback.priv = &info;

	xdl_diff(&old_data, &new_data, &xdiff_params, &xdiff_config, &xdiff_callback);

	return 0;
}
