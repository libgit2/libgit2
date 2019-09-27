/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "mem.h"

#include "futils.h"

int git_mem_from_fd(git_mem *out, git_file fd, git_off_t begin, size_t len)
{
	int error;

	if ((error = p_mmap(&out->map, len, P_MMAP_PROT_READ, P_MMAP_MAP_PRIVATE, fd, begin)) < 0)
		return -1;

	out->data = out->map.data;
	out->len = out->map.len;
	out->type = GIT_MEM_TYPE_MMAP;

	return 0;
}

int git_mem_from_fd_rw(git_mem *out, git_file fd, git_off_t begin, size_t len)
{
	int error;

	if ((error = p_mmap(&out->map, len, P_MMAP_PROT_WRITE, P_MMAP_MAP_SHARED, fd, begin)) < 0)
		return -1;

	out->data = out->map.data;
	out->len = out->map.len;
	out->type = GIT_MEM_TYPE_MMAP;

	return 0;
}

int git_mem_from_path(git_mem *out, const char *path)
{
	git_off_t len;
	git_file fd;
	int error;

	if ((fd = git_futils_open_ro(path)) < 0)
		return fd;

	if ((len = git_futils_filesize(fd)) < 0) {
		error = -1;
		goto out;
	}

	if (!git__is_sizet(len)) {
		git_error_set(GIT_ERROR_OS, "file `%s` too large to mmap", path);
		error = -1;
		goto out;
	}

	error = git_mem_from_fd(out, fd, 0, (size_t)len);
out:
	p_close(fd);
	return error;
}

int git_mem_from_data(git_mem *out, void *data, size_t len)
{
	out->data = data;
	out->len = len;
	out->type = GIT_MEM_TYPE_DATA;
	return 0;
}

int git_mem_from_unowned(git_mem *out, const void *data, size_t len)
{
	out->data = (void *) data;
	out->len = len;
	out->type = GIT_MEM_TYPE_UNOWNED;
	return 0;
}

void git_mem_dispose(git_mem *mem)
{
	if (!mem)
		return;

	switch (mem->type)
	{
	case GIT_MEM_TYPE_MMAP:
		if (mem->map.data)
			p_munmap(&mem->map);
		break;
	case GIT_MEM_TYPE_DATA:
		git__free(mem->data);
		break;
	case GIT_MEM_TYPE_UNOWNED:
		break;
	default:
		assert(0);
	}

	mem->data = NULL;
	mem->len = 0;
}
