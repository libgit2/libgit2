/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "message.h"

static size_t line_length_without_trailing_spaces(const char *line, size_t len)
{
	while (len) {
		unsigned char c = line[len - 1];
		if (!git__isspace(c))
			break;
		len--;
	}

	return len;
}

/* Greatly inspired from git.git "stripspace" */
/* see https://github.com/git/git/blob/497215d8811ac7b8955693ceaad0899ecd894ed2/builtin/stripspace.c#L4-67 */
int git_message__prettify(git_buf *message_out, const char *message, int strip_comments)
{
	const size_t message_len = strlen(message);

	int consecutive_empty_lines = 0;
	size_t i, line_length, rtrimmed_line_length;
	char *next_newline;

	for (i = 0; i < strlen(message); i += line_length) {
		next_newline = memchr(message + i, '\n', message_len - i);

		if (next_newline != NULL) {
			line_length = next_newline - (message + i) + 1;
		} else {
			line_length = message_len - i;
		}

		if (strip_comments && line_length && message[i] == '#')
			continue;

		rtrimmed_line_length = line_length_without_trailing_spaces(message + i, line_length);

		if (!rtrimmed_line_length) {
			consecutive_empty_lines++;
			continue;
		}

		if (consecutive_empty_lines > 0 && message_out->size > 0)
			git_buf_putc(message_out, '\n');

		consecutive_empty_lines = 0;
		git_buf_put(message_out, message + i, rtrimmed_line_length);
		git_buf_putc(message_out, '\n');
	}

	return git_buf_oom(message_out) ? -1 : 0;
}

int git_message_prettify(char *message_out, size_t buffer_size, const char *message, int strip_comments)
{
	git_buf buf = GIT_BUF_INIT;
	ssize_t out_size = -1;

	if (message_out && buffer_size)
		*message_out = '\0';

	if (git_message__prettify(&buf, message, strip_comments) < 0)
		goto done;

	if (message_out && buf.size + 1 > buffer_size) { /* +1 for NUL byte */
		giterr_set(GITERR_INVALID, "Buffer too short to hold the cleaned message");
		goto done;
	}

	if (message_out)
		git_buf_copy_cstr(message_out, buffer_size, &buf);

	out_size = buf.size + 1;

done:
	git_buf_free(&buf);
	return (int)out_size;
}
