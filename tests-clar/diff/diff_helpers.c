#include "diff_helpers.h"

int diff_file_fn(
	void *cb_data,
	git_diff_delta *delta,
	float progress)
{
	diff_expects *e = cb_data;
	(void)progress;
	e->files++;
	if (delta->old_attr == 0)
		e->file_adds++;
	else if (delta->new_attr == 0)
		e->file_dels++;
	else
		e->file_mods++;
	return 0;
}

int diff_hunk_fn(
	void *cb_data,
	git_diff_delta *delta,
	git_diff_range *range,
	const char *header,
	size_t header_len)
{
	diff_expects *e = cb_data;
	(void)delta;
	(void)header;
	(void)header_len;
	e->hunks++;
	e->hunk_old_lines += range->old_lines;
	e->hunk_new_lines += range->new_lines;
	return 0;
}

int diff_line_fn(
	void *cb_data,
	git_diff_delta *delta,
	char line_origin,
	const char *content,
	size_t content_len)
{
	diff_expects *e = cb_data;
	(void)delta;
	(void)content;
	(void)content_len;
	e->lines++;
	switch (line_origin) {
	case GIT_DIFF_LINE_CONTEXT:
		e->line_ctxt++;
		break;
	case GIT_DIFF_LINE_ADDITION:
		e->line_adds++;
		break;
	case GIT_DIFF_LINE_DELETION:
		e->line_dels++;
		break;
	default:
		break;
	}
	return 0;
}
