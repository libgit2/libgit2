/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#include "common.h"
#include "diff.h"
#include "diff_driver.h"
#include "diff_patch.h"
#include "diff_xdiff.h"

static int git_xdiff_scan_int(const char **str, int *value)
{
	const char *scan = *str;
	int v = 0, digits = 0;
	/* find next digit */
	for (scan = *str; *scan && !git__isdigit(*scan); scan++);
	/* parse next number */
	for (; git__isdigit(*scan); scan++, digits++)
		v = (v * 10) + (*scan - '0');
	*str = scan;
	*value = v;
	return (digits > 0) ? 0 : -1;
}

static int git_xdiff_parse_hunk(git_diff_range *range, const char *header)
{
	/* expect something of the form "@@ -%d[,%d] +%d[,%d] @@" */
	if (*header != '@')
		return -1;
	if (git_xdiff_scan_int(&header, &range->old_start) < 0)
		return -1;
	if (*header == ',') {
		if (git_xdiff_scan_int(&header, &range->old_lines) < 0)
			return -1;
	} else
		range->old_lines = 1;
	if (git_xdiff_scan_int(&header, &range->new_start) < 0)
		return -1;
	if (*header == ',') {
		if (git_xdiff_scan_int(&header, &range->new_lines) < 0)
			return -1;
	} else
		range->new_lines = 1;
	if (range->old_start < 0 || range->new_start < 0)
		return -1;

	return 0;
}

typedef struct {
	git_xdiff_output *xo;
	git_diff_patch *patch;
	git_diff_range range;
} git_xdiff_info;

static int git_xdiff_cb(void *priv, mmbuffer_t *bufs, int len)
{
	git_xdiff_info *info = priv;
	git_diff_patch *patch = info->patch;
	const git_diff_delta *delta = git_diff_patch_delta(patch);
	git_diff_output *output = &info->xo->output;

	if (len == 1) {
		output->error = git_xdiff_parse_hunk(&info->range, bufs[0].ptr);
		if (output->error < 0)
			return output->error;

		if (output->hunk_cb != NULL &&
			output->hunk_cb(delta, &info->range,
				bufs[0].ptr, bufs[0].size, output->payload))
			output->error = GIT_EUSER;
	}

	if (len == 2 || len == 3) {
		/* expect " "/"-"/"+", then data */
		char origin =
			(*bufs[0].ptr == '+') ? GIT_DIFF_LINE_ADDITION :
			(*bufs[0].ptr == '-') ? GIT_DIFF_LINE_DELETION :
			GIT_DIFF_LINE_CONTEXT;

		if (output->data_cb != NULL &&
			output->data_cb(delta, &info->range,
				origin, bufs[1].ptr, bufs[1].size, output->payload))
			output->error = GIT_EUSER;
	}

	if (len == 3 && !output->error) {
		/* If we have a '+' and a third buf, then we have added a line
		 * without a newline and the old code had one, so DEL_EOFNL.
		 * If we have a '-' and a third buf, then we have removed a line
		 * with out a newline but added a blank line, so ADD_EOFNL.
		 */
		char origin =
			(*bufs[0].ptr == '+') ? GIT_DIFF_LINE_DEL_EOFNL :
			(*bufs[0].ptr == '-') ? GIT_DIFF_LINE_ADD_EOFNL :
			GIT_DIFF_LINE_CONTEXT_EOFNL;

		if (output->data_cb != NULL &&
			output->data_cb(delta, &info->range,
				origin, bufs[2].ptr, bufs[2].size, output->payload))
			output->error = GIT_EUSER;
	}

	return output->error;
}

static int git_xdiff(git_diff_output *output, git_diff_patch *patch)
{
	git_xdiff_output *xo = (git_xdiff_output *)output;
	git_xdiff_info info;
	git_diff_find_context_payload findctxt;
	mmfile_t xd_old_data, xd_new_data;

	memset(&info, 0, sizeof(info));
	info.patch = patch;
	info.xo    = xo;

	xo->callback.priv = &info;

	git_diff_find_context_init(
		&xo->config.find_func, &findctxt, git_diff_patch__driver(patch));
	xo->config.find_func_priv = &findctxt;

	if (xo->config.find_func != NULL)
		xo->config.flags |= XDL_EMIT_FUNCNAMES;
	else
		xo->config.flags &= ~XDL_EMIT_FUNCNAMES;

	/* TODO: check ofile.opts_flags to see if driver-specific per-file
	 * updates are needed to xo->params.flags
	 */

	git_diff_patch__old_data(&xd_old_data.ptr, &xd_old_data.size, patch);
	git_diff_patch__new_data(&xd_new_data.ptr, &xd_new_data.size, patch);

	xdl_diff(&xd_old_data, &xd_new_data,
		&xo->params, &xo->config, &xo->callback);

	git_diff_find_context_clear(&findctxt);

	return xo->output.error;
}

void git_xdiff_init(git_xdiff_output *xo, const git_diff_options *opts)
{
	uint32_t flags = opts ? opts->flags : GIT_DIFF_NORMAL;

	xo->output.diff_cb = git_xdiff;

	memset(&xo->config, 0, sizeof(xo->config));
	xo->config.ctxlen = opts ? opts->context_lines : 3;
	xo->config.interhunkctxlen = opts ? opts->interhunk_lines : 0;

	memset(&xo->params, 0, sizeof(xo->params));
	if (flags & GIT_DIFF_IGNORE_WHITESPACE)
		xo->params.flags |= XDF_WHITESPACE_FLAGS;
	if (flags & GIT_DIFF_IGNORE_WHITESPACE_CHANGE)
		xo->params.flags |= XDF_IGNORE_WHITESPACE_CHANGE;
	if (flags & GIT_DIFF_IGNORE_WHITESPACE_EOL)
		xo->params.flags |= XDF_IGNORE_WHITESPACE_AT_EOL;

	memset(&xo->callback, 0, sizeof(xo->callback));
	xo->callback.outf = git_xdiff_cb;
}
