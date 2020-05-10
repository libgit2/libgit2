/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "oid.h"

#include "git2/oid.h"
#include <string.h>
#include <limits.h>

char *git_oid_allocfmt(const git_oid *oid)
{
	char *str = git__malloc(GIT_OID_HEXSZ + 1);
	if (!str)
		return NULL;
	git_oid_nfmt(str, GIT_OID_HEXSZ + 1, oid);
	return str;
}

int git_oid__parse(
	git_oid *oid, const char **buffer_out,
	const char *buffer_end, const char *header)
{
	const size_t sha_len = GIT_OID_HEXSZ;
	const size_t header_len = strlen(header);

	const char *buffer = *buffer_out;

	if (buffer + (header_len + sha_len + 1) > buffer_end)
		return -1;

	if (memcmp(buffer, header, header_len) != 0)
		return -1;

	if (buffer[header_len + sha_len] != '\n')
		return -1;

	if (git_oid_fromstr(oid, buffer + header_len) < 0)
		return -1;

	*buffer_out = buffer + (header_len + sha_len + 1);

	return 0;
}

void git_oid__writebuf(git_buf *buf, const char *header, const git_oid *oid)
{
	char hex_oid[GIT_OID_HEXSZ];

	git_oid_fmt(hex_oid, oid);
	git_buf_puts(buf, header);
	git_buf_put(buf, hex_oid, GIT_OID_HEXSZ);
	git_buf_putc(buf, '\n');
}
