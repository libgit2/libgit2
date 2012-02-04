/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "git2/diff.h"
#include "diff.h"
#include "xdiff/xdiff.h"
#include "blob.h"
#include <ctype.h>

static git_diff_delta *file_delta_new(
	git_diff_list *diff,
	const git_tree_diff_data *tdiff)
{
	git_diff_delta *delta = git__calloc(1, sizeof(git_diff_delta));

	if (!delta) {
		git__rethrow(GIT_ENOMEM, "Could not allocate diff record");
		return NULL;
	}

	/* copy shared fields */
	delta->status   = tdiff->status;
	delta->old_attr = tdiff->old_attr;
	delta->new_attr = tdiff->new_attr;
	delta->old_oid  = tdiff->old_oid;
	delta->new_oid  = tdiff->new_oid;
	delta->path     = git__strdup(diff->pfx.ptr);
	if (delta->path == NULL) {
		git__free(delta);
		git__rethrow(GIT_ENOMEM, "Could not allocate diff record path");
		return NULL;
	}

	return delta;
}

static void file_delta_free(git_diff_delta *delta)
{
	if (!delta)
		return;

	if (delta->new_path != delta->path) {
		git__free((char *)delta->new_path);
		delta->new_path = NULL;
	}

	git__free((char *)delta->path);
	delta->path = NULL;

	git__free(delta);
}

static int tree_add_cb(const char *root, git_tree_entry *entry, void *data)
{
	int error;
	git_diff_list *diff = data;
	ssize_t pfx_len = diff->pfx.size;
	git_tree_diff_data tdiff;
	git_diff_delta *delta;

	memset(&tdiff, 0, sizeof(tdiff));
	tdiff.new_attr = git_tree_entry_attributes(entry);
	if (S_ISDIR(tdiff.new_attr))
		return GIT_SUCCESS;

	git_oid_cpy(&tdiff.new_oid, git_tree_entry_id(entry));
	tdiff.status = GIT_STATUS_ADDED;
	tdiff.path = git_tree_entry_name(entry);

	if ((error = git_buf_joinpath(&diff->pfx, diff->pfx.ptr, root)) ||
		(error = git_buf_joinpath(&diff->pfx, diff->pfx.ptr, tdiff.path)))
		return error;

	delta = file_delta_new(diff, &tdiff);
	if (delta  == NULL)
		error = GIT_ENOMEM;
	else if ((error = git_vector_insert(&diff->files, delta)) < GIT_SUCCESS)
		file_delta_free(delta);

	git_buf_truncate(&diff->pfx, pfx_len);

	return error;
}

static int tree_del_cb(const char *root, git_tree_entry *entry, void *data)
{
	int error;
	git_diff_list *diff = data;
	ssize_t pfx_len = diff->pfx.size;
	git_tree_diff_data tdiff;
	git_diff_delta *delta;

	memset(&tdiff, 0, sizeof(tdiff));
	tdiff.old_attr = git_tree_entry_attributes(entry);
	if (S_ISDIR(tdiff.old_attr))
		return GIT_SUCCESS;

	git_oid_cpy(&tdiff.old_oid, git_tree_entry_id(entry));
	tdiff.status = GIT_STATUS_DELETED;
	tdiff.path = git_tree_entry_name(entry);

	if ((error = git_buf_joinpath(&diff->pfx, diff->pfx.ptr, root)) ||
		(error = git_buf_joinpath(&diff->pfx, diff->pfx.ptr, tdiff.path)))
		return error;

	delta = file_delta_new(diff, &tdiff);
	if (delta  == NULL)
		error = GIT_ENOMEM;
	else if ((error = git_vector_insert(&diff->files, delta)) < GIT_SUCCESS)
		file_delta_free(delta);

	git_buf_truncate(&diff->pfx, pfx_len);

	return error;
}

static int tree_diff_cb(const git_tree_diff_data *ptr, void *data)
{
	int error;
	git_diff_list *diff = data;
	ssize_t pfx_len = diff->pfx.size;

	error = git_buf_joinpath(&diff->pfx, diff->pfx.ptr, ptr->path);
	if (error < GIT_SUCCESS)
		return error;

	/* there are 4 tree related cases:
	 * - diff tree to tree, which just means we recurse
	 * - tree was deleted
	 * - tree was added
	 * - tree became non-tree or vice versa, which git_tree_diff
	 *   will already have converted into two calls: an addition
	 *   and a deletion (thank you, git_tree_diff!)
	 * otherwise, this is a blob-to-blob diff
	 */
	if (S_ISDIR(ptr->old_attr) && S_ISDIR(ptr->new_attr)) {
		git_tree *old = NULL, *new = NULL;

		if (!(error = git_tree_lookup(&old, diff->repo, &ptr->old_oid)) &&
			!(error = git_tree_lookup(&new, diff->repo, &ptr->new_oid)))
		{
			error = git_tree_diff(old, new, tree_diff_cb, diff);
		}

		git_tree_free(old);
		git_tree_free(new);
	} else if (S_ISDIR(ptr->old_attr) && ptr->new_attr == 0) {
		/* deleted a whole tree */
		git_tree *old = NULL;
		if (!(error = git_tree_lookup(&old, diff->repo, &ptr->old_oid))) {
			error = git_tree_walk(old, tree_del_cb, GIT_TREEWALK_POST, diff);
			git_tree_free(old);
		}
	} else if (S_ISDIR(ptr->new_attr) && ptr->old_attr == 0) {
		/* added a whole tree */
		git_tree *new = NULL;
		if (!(error = git_tree_lookup(&new, diff->repo, &ptr->new_oid))) {
			error = git_tree_walk(new, tree_add_cb, GIT_TREEWALK_POST, diff);
			git_tree_free(new);
		}
	} else {
		git_diff_delta *delta = file_delta_new(diff, ptr);
		if (delta == NULL)
			error = GIT_ENOMEM;
		else if ((error = git_vector_insert(&diff->files, delta)) < GIT_SUCCESS)
			file_delta_free(delta);
	}

	git_buf_truncate(&diff->pfx, pfx_len);

	return error;
}

static git_diff_list *git_diff_list_alloc(
	git_repository *repo, const git_diff_options *opts)
{
	git_diff_list *diff = git__calloc(1, sizeof(git_diff_list));
	if (diff != NULL) {
		if (opts != NULL) {
			memcpy(&diff->opts, opts, sizeof(git_diff_options));
			/* do something safer with the pathspec strarray */
		}
		diff->repo = repo;
		git_buf_init(&diff->pfx, 0);
	}
	return diff;
}

void git_diff_list_free(git_diff_list *diff)
{
	git_diff_delta *delta;
	unsigned int i;

	if (!diff)
		return;

	git_buf_free(&diff->pfx);
	git_vector_foreach(&diff->files, i, delta) {
		file_delta_free(delta);
		diff->files.contents[i] = NULL;
	}
	git_vector_free(&diff->files);
	git__free(diff);
}

int git_diff_tree_to_tree(
	git_repository *repo,
	const git_diff_options *opts,
	git_tree *old,
	git_tree *new,
	git_diff_list **diff_ptr)
{
	int error;
	git_diff_list *diff = git_diff_list_alloc(repo, opts);
	if (!diff)
		return GIT_ENOMEM;

	if ((error = git_tree_diff(old, new, tree_diff_cb, diff)) == GIT_SUCCESS) {
		git_buf_free(&diff->pfx);	/* don't need this anymore */
		*diff_ptr = diff;
	} else
		git_diff_list_free(diff);

	return error;
}

typedef struct {
	git_diff_list *diff;
	void *cb_data;
	git_diff_hunk_fn hunk_cb;
	git_diff_line_fn line_cb;
	unsigned int index;
	git_diff_delta *delta;
} diff_info;

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
	return (digits > 0) ? GIT_SUCCESS : GIT_ENOTFOUND;
}

static int diff_output_cb(void *priv, mmbuffer_t *bufs, int len)
{
	int err = GIT_SUCCESS;
	diff_info *di = priv;

	if (len == 1 && di->hunk_cb) {
		git_diff_range range = { -1, 0, -1, 0 };

		/* expect something of the form "@@ -%d[,%d] +%d[,%d] @@" */
		if (bufs[0].ptr[0] == '@') {
			const char *scan = bufs[0].ptr;
			if (!(err = read_next_int(&scan, &range.old_start)) && *scan == ',')
				err = read_next_int(&scan, &range.old_lines);
			if (!err &&
				!(err = read_next_int(&scan, &range.new_start)) && *scan == ',')
				err = read_next_int(&scan, &range.new_lines);
			if (!err && range.old_start >= 0 && range.new_start >= 0)
				err = di->hunk_cb(
					di->cb_data, di->delta, &range, bufs[0].ptr, bufs[0].size);
		}
	}
	else if ((len == 2 || len == 3) && di->line_cb) {
		int origin;

		/* expect " "/"-"/"+", then data, then maybe newline */
		origin =
			(*bufs[0].ptr == '+') ? GIT_DIFF_LINE_ADDITION :
			(*bufs[0].ptr == '-') ? GIT_DIFF_LINE_DELETION :
			GIT_DIFF_LINE_CONTEXT;

		err = di->line_cb(
			di->cb_data, di->delta, origin, bufs[1].ptr, bufs[1].size);

		/* deal with adding and removing newline at EOF */
		if (err == GIT_SUCCESS && len == 3) {
			if (origin == GIT_DIFF_LINE_ADDITION)
				origin = GIT_DIFF_LINE_ADD_EOFNL;
			else
				origin = GIT_DIFF_LINE_DEL_EOFNL;

			err = di->line_cb(
				di->cb_data, di->delta, origin, bufs[2].ptr, bufs[2].size);
		}
	}

	return err;
}

static int set_file_is_binary(
	git_repository *repo,
	git_diff_delta *file,
	mmfile_t *old,
	mmfile_t *new)
{
	int error;
	const char *value;

	/* check diff attribute +, -, or 0 */
	error = git_attr_get(repo, file->path, "diff", &value);
	if (error != GIT_SUCCESS)
		return error;

	if (value == GIT_ATTR_TRUE) {
		file->binary = 0;
		return GIT_SUCCESS;
	}
	if (value == GIT_ATTR_FALSE) {
		file->binary = 1;
		return GIT_SUCCESS;
	}

	/* TODO: if value != NULL, implement diff drivers */
	/* TODO: check if NUL byte appears in first bit */
	GIT_UNUSED_ARG(old);
	GIT_UNUSED_ARG(new);
	file->binary = 0;
	return GIT_SUCCESS;
}

int git_diff_foreach(
	git_diff_list *diff,
	void *data,
	git_diff_file_fn file_cb,
	git_diff_hunk_fn hunk_cb,
	git_diff_line_fn line_cb)
{
	int error = GIT_SUCCESS;
	diff_info di;
	git_diff_delta *delta;

	di.diff    = diff;
	di.cb_data = data;
	di.hunk_cb = hunk_cb;
	di.line_cb = line_cb;

	git_vector_foreach(&diff->files, di.index, delta) {
		mmfile_t old, new;
		xpparam_t params;
		xdemitconf_t cfg;
		xdemitcb_t callback;

		/* map files */
		if (hunk_cb || line_cb) {
			/* TODO: Partial blob reading to defer loading whole blob.
			 * I.e. I want a blob with just the first 4kb loaded, then
			 * later on I will read the rest of the blob if needed.
			 */

			if (delta->status == GIT_STATUS_DELETED ||
				delta->status == GIT_STATUS_MODIFIED)
			{
				error = git_blob_lookup(
					&delta->old_blob, diff->repo, &delta->old_oid);
				old.ptr = (char *)git_blob_rawcontent(delta->old_blob);
				old.size = git_blob_rawsize(delta->old_blob);
			} else {
				delta->old_blob = NULL;
				old.ptr = "";
				old.size = 0;
			}

			if (delta->status == GIT_STATUS_ADDED ||
				delta->status == GIT_STATUS_MODIFIED)
			{
				error = git_blob_lookup(
					&delta->new_blob, diff->repo, &delta->new_oid);
				new.ptr = (char *)git_blob_rawcontent(delta->new_blob);
				new.size = git_blob_rawsize(delta->new_blob);
			} else {
				delta->new_blob = NULL;
				new.ptr = "";
				new.size = 0;
			}
		}

		if (diff->opts.force_text)
			delta->binary = 0;
		else if ((error = set_file_is_binary(
			diff->repo, delta, &old, &new)) < GIT_SUCCESS)
			break;

		if (file_cb != NULL) {
			error = file_cb(data, delta, (float)di.index / diff->files.length);
			if (error != GIT_SUCCESS)
				break;
		}

		/* don't do hunk and line diffs if file is binary */
		if (delta->binary)
			continue;

		/* nothing to do if we did not get a blob */
		if (!delta->old_blob && !delta->new_blob)
			continue;

		assert(hunk_cb || line_cb);

		di.delta = delta;

		memset(&params, 0, sizeof(params));

		memset(&cfg, 0, sizeof(cfg));
		cfg.ctxlen = diff->opts.context_lines || 3;
		cfg.interhunkctxlen = diff->opts.interhunk_lines || 3;
		if (diff->opts.ignore_whitespace)
			cfg.flags |= XDF_WHITESPACE_FLAGS;

		memset(&callback, 0, sizeof(callback));
		callback.outf = diff_output_cb;
		callback.priv = &di;

		xdl_diff(&old, &new, &params, &cfg, &callback);

		git_blob_free(delta->old_blob);
		delta->old_blob = NULL;

		git_blob_free(delta->new_blob);
		delta->new_blob = NULL;
	}

	return error;
}

typedef struct {
	git_diff_output_fn print_cb;
	void *cb_data;
	git_buf *buf;
} print_info;

static char pick_suffix(int mode)
{
	if (S_ISDIR(mode))
		return '/';
	else if (mode & 0100)
		/* modes in git are not very flexible, so if this bit is set,
		 * we must be dealwith with a 100755 type of file.
		 */
		return '*';
	else
		return ' ';
}

static int print_compact(void *data, git_diff_delta *delta, float progress)
{
	print_info *pi = data;
	char code, old_suffix, new_suffix;

	GIT_UNUSED_ARG(progress);

	switch (delta->status) {
	case GIT_STATUS_ADDED: code = 'A'; break;
	case GIT_STATUS_DELETED: code = 'D'; break;
	case GIT_STATUS_MODIFIED: code = 'M'; break;
	case GIT_STATUS_RENAMED: code = 'R'; break;
	case GIT_STATUS_COPIED: code = 'C'; break;
	case GIT_STATUS_IGNORED: code = 'I'; break;
	case GIT_STATUS_UNTRACKED: code = '?'; break;
	default: code = 0;
	}

	if (!code)
		return GIT_SUCCESS;

	old_suffix = pick_suffix(delta->old_attr);
	new_suffix = pick_suffix(delta->new_attr);

	git_buf_clear(pi->buf);

	if (delta->new_path != NULL)
		git_buf_printf(pi->buf, "%c\t%s%c -> %s%c\n", code,
			delta->path, old_suffix, delta->new_path, new_suffix);
	else if (delta->old_attr != delta->new_attr)
		git_buf_printf(pi->buf, "%c\t%s%c (%o -> %o)\n", code,
			delta->path, new_suffix, delta->old_attr, delta->new_attr);
	else if (old_suffix != ' ')
		git_buf_printf(pi->buf, "%c\t%s%c\n", code, delta->path, old_suffix);
	else
		git_buf_printf(pi->buf, "%c\t%s\n", code, delta->path);

	if (git_buf_lasterror(pi->buf) != GIT_SUCCESS)
		return git_buf_lasterror(pi->buf);

	return pi->print_cb(pi->cb_data, GIT_DIFF_LINE_FILE_HDR, pi->buf->ptr);
}

int git_diff_print_compact(
	git_diff_list *diff,
	void *cb_data,
	git_diff_output_fn print_cb)
{
	int error;
	git_buf buf = GIT_BUF_INIT;
	print_info pi;

	pi.print_cb = print_cb;
	pi.cb_data  = cb_data;
	pi.buf      = &buf;

	error = git_diff_foreach(diff, &pi, print_compact, NULL, NULL);

	git_buf_free(&buf);

	return error;
}


static int print_oid_range(print_info *pi, git_diff_delta *delta)
{
	char start_oid[8], end_oid[8];

	/* TODO: Determine a good actual OID range to print */
	git_oid_to_string(start_oid, sizeof(start_oid), &delta->old_oid);
	git_oid_to_string(end_oid, sizeof(end_oid), &delta->new_oid);

	/* TODO: Match git diff more closely */
	if (delta->old_attr == delta->new_attr) {
		git_buf_printf(pi->buf, "index %s..%s %o\n",
			start_oid, end_oid, delta->old_attr);
	} else {
		if (delta->old_attr == 0) {
			git_buf_printf(pi->buf, "new file mode %o\n", delta->new_attr);
		} else if (delta->new_attr == 0) {
			git_buf_printf(pi->buf, "deleted file mode %o\n", delta->old_attr);
		} else {
			git_buf_printf(pi->buf, "old mode %o\n", delta->old_attr);
			git_buf_printf(pi->buf, "new mode %o\n", delta->new_attr);
		}
		git_buf_printf(pi->buf, "index %s..%s\n", start_oid, end_oid);
	}

	return git_buf_lasterror(pi->buf);
}

static int print_patch_file(void *data, git_diff_delta *delta, float progress)
{
	int error;
	print_info *pi = data;
	const char *oldpfx = "a/";
	const char *oldpath = delta->path;
	const char *newpfx = "b/";
	const char *newpath = delta->new_path ? delta->new_path : delta->path;

	GIT_UNUSED_ARG(progress);

	git_buf_clear(pi->buf);
	git_buf_printf(pi->buf, "diff --git a/%s b/%s\n", delta->path, newpath);
	if ((error = print_oid_range(pi, delta)) < GIT_SUCCESS)
		return error;

	if (delta->old_blob == NULL) {
		oldpfx = "";
		oldpath = "/dev/null";
	}
	if (delta->new_blob == NULL) {
		oldpfx = "";
		oldpath = "/dev/null";
	}

	if (!delta->binary) {
		git_buf_printf(pi->buf, "--- %s%s\n", oldpfx, oldpath);
		git_buf_printf(pi->buf, "+++ %s%s\n", newpfx, newpath);
	}

	if (git_buf_lasterror(pi->buf) != GIT_SUCCESS)
		return git_buf_lasterror(pi->buf);

	error = pi->print_cb(pi->cb_data, GIT_DIFF_LINE_FILE_HDR, pi->buf->ptr);
	if (error != GIT_SUCCESS || !delta->binary)
		return error;

	git_buf_clear(pi->buf);
	git_buf_printf(
		pi->buf, "Binary files %s%s and %s%s differ\n",
		oldpfx, oldpath, newpfx, newpath);
	if (git_buf_lasterror(pi->buf) != GIT_SUCCESS)
		return git_buf_lasterror(pi->buf);

	return pi->print_cb(pi->cb_data, GIT_DIFF_LINE_BINARY, pi->buf->ptr);
}

static int print_patch_hunk(
	void *data,
	git_diff_delta *d,
	git_diff_range *r,
	const char *header,
	size_t header_len)
{
	print_info *pi = data;

	GIT_UNUSED_ARG(d);
	GIT_UNUSED_ARG(r);

	git_buf_clear(pi->buf);

	if (git_buf_printf(pi->buf, "%.*s", (int)header_len, header) == GIT_SUCCESS)
		return pi->print_cb(pi->cb_data, GIT_DIFF_LINE_HUNK_HDR, pi->buf->ptr);
	else
		return git_buf_lasterror(pi->buf);
}

static int print_patch_line(
	void *data,
	git_diff_delta *delta,
	char line_origin, /* GIT_DIFF_LINE value from above */
	const char *content,
	size_t content_len)
{
	print_info *pi = data;

	GIT_UNUSED_ARG(delta);

	git_buf_clear(pi->buf);

	if (line_origin == GIT_DIFF_LINE_ADDITION ||
		line_origin == GIT_DIFF_LINE_DELETION ||
		line_origin == GIT_DIFF_LINE_CONTEXT)
		git_buf_printf(pi->buf, "%c%.*s", line_origin, (int)content_len, content);
	else if (content_len > 0)
		git_buf_printf(pi->buf, "%.*s", (int)content_len, content);

	if (git_buf_lasterror(pi->buf) != GIT_SUCCESS)
		return git_buf_lasterror(pi->buf);

	return pi->print_cb(pi->cb_data, line_origin, pi->buf->ptr);
}

int git_diff_print_patch(
	git_diff_list *diff,
	void *cb_data,
	git_diff_output_fn print_cb)
{
	int error;
	git_buf buf = GIT_BUF_INIT;
	print_info pi;

	pi.print_cb = print_cb;
	pi.cb_data  = cb_data;
	pi.buf      = &buf;

	error = git_diff_foreach(
		diff, &pi, print_patch_file, print_patch_hunk, print_patch_line);

	git_buf_free(&buf);

	return error;
}

int git_diff_blobs(
	git_repository *repo,
	git_blob *old_blob,
	git_blob *new_blob,
	git_diff_options *options,
	void *cb_data,
	git_diff_hunk_fn hunk_cb,
	git_diff_line_fn line_cb)
{
	diff_info di;
	git_diff_delta delta;
	mmfile_t old, new;
	xpparam_t params;
	xdemitconf_t cfg;
	xdemitcb_t callback;

	assert(repo && options);

	if (old_blob) {
		old.ptr  = (char *)git_blob_rawcontent(old_blob);
		old.size = git_blob_rawsize(old_blob);
	} else {
		old.ptr  = "";
		old.size = 0;
	}

	if (new_blob) {
		new.ptr  = (char *)git_blob_rawcontent(new_blob);
		new.size = git_blob_rawsize(new_blob);
	} else {
		new.ptr  = "";
		new.size = 0;
	}

	/* populate a "fake" delta record */
	delta.status = old.ptr ?
		(new.ptr ? GIT_STATUS_MODIFIED : GIT_STATUS_DELETED) :
		(new.ptr ? GIT_STATUS_ADDED : GIT_STATUS_UNTRACKED);
	delta.old_attr = 0100644; /* can't know the truth from a blob alone */
	delta.new_attr = 0100644;
	git_oid_cpy(&delta.old_oid, git_object_id((const git_object *)old_blob));
	git_oid_cpy(&delta.new_oid, git_object_id((const git_object *)new_blob));
	delta.old_blob = old_blob;
	delta.new_blob = new_blob;
	delta.path = NULL;
	delta.new_path = NULL;
	delta.similarity = 0;
	delta.binary = 0;

	di.diff    = NULL;
	di.delta   = &delta;
	di.cb_data = cb_data;
	di.hunk_cb = hunk_cb;
	di.line_cb = line_cb;	

	memset(&params, 0, sizeof(params));

	memset(&cfg, 0, sizeof(cfg));
	cfg.ctxlen = options->context_lines || 3;
	cfg.interhunkctxlen = options->interhunk_lines || 3;
	if (options->ignore_whitespace)
		cfg.flags |= XDF_WHITESPACE_FLAGS;

	memset(&callback, 0, sizeof(callback));
	callback.outf = diff_output_cb;
	callback.priv = &di;

	xdl_diff(&old, &new, &params, &cfg, &callback);

	return GIT_SUCCESS;
}
