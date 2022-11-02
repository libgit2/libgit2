/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#ifndef INCLUDE_chunk_format_h__
#define INCLUDE_chunk_format_h__

#include "common.h"

#include "git2/types.h"

/*
 * The chunk format is a common set of substructures in some Git formats.
 * This file declares some helpful methods for writing and reading these
 * formats to help share code across different file types.
 *
 * See https://git-scm.com/docs/chunk-format for details on the chunk
 * format, including how it uses a table of contents to describe distinct
 * sections of structured data within a file.
 */

typedef int (*chunk_format_write_cb)(const char *buf, size_t size, void *cb_data);

int write_offset(off64_t offset, chunk_format_write_cb write_cb, void *cb_data);

int write_chunk_header(
		int chunk_id,
		off64_t offset,
		chunk_format_write_cb write_cb,
		void *cb_data);

#endif
