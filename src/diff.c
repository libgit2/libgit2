/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "git2/diff.h"
#include "xdiff/xdiff.h"
#include "blob.h"
#include <ctype.h>

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
	git_diff_opts *opts = priv;

	if (len == 1) {
		int ostart = -1, olen = 0, nstart = -1, nlen = 0;
		/* expect something of the form "@@ -%d[,%d] +%d[,%d] @@" */
		if (opts->hunk_cb && bufs[0].ptr[0] == '@') {
			const char *scan = bufs[0].ptr;
			if (!(err = read_next_int(&scan, &ostart)) && *scan == ',')
				err = read_next_int(&scan, &olen);
			if (!err && !(err = read_next_int(&scan, &nstart)) && *scan == ',')
				err = read_next_int(&scan, &nlen);
			if (!err && ostart >= 0 && nstart >= 0)
				err = opts->hunk_cb(
					opts->cb_data, ostart, olen, nstart, nlen);
		}
	}
	else if (len == 2 || len == 3) {
		int origin;
		/* expect " "/"-"/"+", then data, then maybe newline */
		origin =
			(*bufs[0].ptr == '+') ? GIT_DIFF_LINE_ADDITION :
			(*bufs[0].ptr == '-') ? GIT_DIFF_LINE_DELETION :
			GIT_DIFF_LINE_CONTEXT;

		if (opts->line_cb)
			err = opts->line_cb(
				opts->cb_data, origin, bufs[1].ptr, bufs[1].size);
	}

	return err;
}

int git_diff_blobs(
	git_repository *repo,
	git_blob *old_blob,
	git_blob *new_blob,
	git_diff_opts *options)
{
	mmfile_t old, new;
	xpparam_t params;
	xdemitconf_t cfg;
	xdemitcb_t callback;

	assert(repo && old_blob && new_blob && options);

	old.ptr  = (char *)git_blob_rawcontent(old_blob);
	old.size = git_blob_rawsize(old_blob);

	new.ptr  = (char *)git_blob_rawcontent(new_blob);
	new.size = git_blob_rawsize(new_blob);

	memset(&params, 0, sizeof(params));

	memset(&cfg, 0, sizeof(cfg));
	cfg.ctxlen = options->context_lines || 3;
	cfg.interhunkctxlen = options->interhunk_lines || 3;
	if (options->ignore_whitespace)
		cfg.flags |= XDF_WHITESPACE_FLAGS;

	memset(&callback, 0, sizeof(callback));
	callback.outf = diff_output_cb;
	callback.priv = options;

	if (options->file_cb)
		options->file_cb(
			options->cb_data,
			git_object_id((const git_object *)old_blob), NULL, 010644, 
			git_object_id((const git_object *)new_blob), NULL, 010644);

	xdl_diff(&old, &new, &params, &cfg, &callback);

	return GIT_SUCCESS;
}

