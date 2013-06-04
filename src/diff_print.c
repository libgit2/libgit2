/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#include "common.h"
#include "diff.h"
#include "diff_output.h"

typedef struct {
	git_diff_list *diff;
	git_diff_data_cb print_cb;
	void *payload;
	git_buf *buf;
	int oid_strlen;
} diff_print_info;

static int diff_print_info_init(
	diff_print_info *pi,
	git_buf *out, git_diff_list *diff, git_diff_data_cb cb, void *payload)
{
	assert(diff && diff->repo);

	pi->diff     = diff;
	pi->print_cb = cb;
	pi->payload  = payload;
	pi->buf      = out;

	if (git_repository__cvar(&pi->oid_strlen, diff->repo, GIT_CVAR_ABBREV) < 0)
		return -1;

	pi->oid_strlen += 1; /* for NUL byte */

	if (pi->oid_strlen < 2)
		pi->oid_strlen = 2;
	else if (pi->oid_strlen > GIT_OID_HEXSZ + 1)
		pi->oid_strlen = GIT_OID_HEXSZ + 1;

	return 0;
}

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

static int callback_error(void)
{
	giterr_clear();
	return GIT_EUSER;
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
		return callback_error();

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

	if (!(error = diff_print_info_init(&pi, &buf, diff, print_cb, payload)))
		error = git_diff_foreach(diff, print_compact, NULL, NULL, &pi);

	git_buf_free(&buf);

	return error;
}

static int print_raw(
	const git_diff_delta *delta, float progress, void *data)
{
	diff_print_info *pi = data;
	char code = git_diff_status_char(delta->status);
	char start_oid[GIT_OID_HEXSZ+1], end_oid[GIT_OID_HEXSZ+1];

	GIT_UNUSED(progress);

	if (code == ' ')
		return 0;

	git_buf_clear(pi->buf);

	git_oid_tostr(start_oid, pi->oid_strlen, &delta->old_file.oid);
	git_oid_tostr(end_oid, pi->oid_strlen, &delta->new_file.oid);

	git_buf_printf(
		pi->buf, ":%06o %06o %s... %s... %c",
		delta->old_file.mode, delta->new_file.mode, start_oid, end_oid, code);

	if (delta->similarity > 0)
		git_buf_printf(pi->buf, "%03u", delta->similarity);

	if (delta->status == GIT_DELTA_RENAMED || delta->status == GIT_DELTA_COPIED)
		git_buf_printf(
			pi->buf, "\t%s %s\n", delta->old_file.path, delta->new_file.path);
	else
		git_buf_printf(
			pi->buf, "\t%s\n", delta->old_file.path ?
			delta->old_file.path : delta->new_file.path);

	if (git_buf_oom(pi->buf))
		return -1;

	if (pi->print_cb(delta, NULL, GIT_DIFF_LINE_FILE_HDR,
			git_buf_cstr(pi->buf), git_buf_len(pi->buf), pi->payload))
		return callback_error();

	return 0;
}

int git_diff_print_raw(
	git_diff_list *diff,
	git_diff_data_cb print_cb,
	void *payload)
{
	int error;
	git_buf buf = GIT_BUF_INIT;
	diff_print_info pi;

	if (!(error = diff_print_info_init(&pi, &buf, diff, print_cb, payload)))
		error = git_diff_foreach(diff, print_raw, NULL, NULL, &pi);

	git_buf_free(&buf);

	return error;
}

static int print_oid_range(diff_print_info *pi, const git_diff_delta *delta)
{
	char start_oid[GIT_OID_HEXSZ+1], end_oid[GIT_OID_HEXSZ+1];

	git_oid_tostr(start_oid, pi->oid_strlen, &delta->old_file.oid);
	git_oid_tostr(end_oid, pi->oid_strlen, &delta->new_file.oid);

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

	if (S_ISDIR(delta->new_file.mode) ||
		delta->status == GIT_DELTA_UNMODIFIED ||
		delta->status == GIT_DELTA_IGNORED ||
		(delta->status == GIT_DELTA_UNTRACKED &&
		 (pi->diff->opts.flags & GIT_DIFF_INCLUDE_UNTRACKED_CONTENT) == 0))
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
		return callback_error();

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
		return callback_error();

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
		return callback_error();

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
		return callback_error();

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

	if (!(error = diff_print_info_init(&pi, &buf, diff, print_cb, payload)))
		error = git_diff_foreach(
			diff, print_patch_file, print_patch_hunk, print_patch_line, &pi);

	git_buf_free(&buf);

	return error;
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

	if (!(error = diff_print_info_init(
			&pi, &temp, patch->diff, print_cb, payload)))
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
