/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#ifndef INCLUDE_mem_h__
#define INCLUDE_mem_h__

#include "common.h"

#include "map.h"
#include "posix.h"

typedef enum {
	GIT_MEM_TYPE_MMAP,
	GIT_MEM_TYPE_DATA,
	GIT_MEM_TYPE_UNOWNED
} git_mem_type;

typedef struct {
	void *data;
	size_t len;
	git_map map;
	git_mem_type type;
} git_mem;

int git_mem_from_fd(git_mem *out, git_file fd, git_off_t begin, size_t len);
int git_mem_from_fd_rw(git_mem *out, git_file fd, git_off_t begin, size_t len);
int git_mem_from_path(git_mem *out, const char *path);
int git_mem_from_data(git_mem *out, void *data, size_t len);
int git_mem_from_unowned(git_mem *out, const void *data, size_t len);
void git_mem_dispose(git_mem *mem);

#endif /* INCLUDE_mem_h__ */
