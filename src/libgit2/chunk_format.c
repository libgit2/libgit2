/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "chunk_format.h"

int write_offset(off64_t offset, chunk_format_write_cb write_cb, void *cb_data)
{
	int error;
	uint32_t word;

	word = htonl((uint32_t)((offset >> 32) & 0xffffffffu));
	error = write_cb((const char *)&word, sizeof(word), cb_data);
	if (error < 0)
		return error;
	word = htonl((uint32_t)((offset >> 0) & 0xffffffffu));
	error = write_cb((const char *)&word, sizeof(word), cb_data);
	if (error < 0)
		return error;

	return 0;
}

int write_chunk_header(
		int chunk_id,
		off64_t offset,
		chunk_format_write_cb write_cb,
		void *cb_data)
{
	uint32_t word = htonl(chunk_id);
	int error = write_cb((const char *)&word, sizeof(word), cb_data);
	if (error < 0)
		return error;
	return write_offset(offset, write_cb, cb_data);
}
