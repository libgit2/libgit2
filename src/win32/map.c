/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "map.h"
#include <errno.h>

#ifndef NO_MMAP

static DWORD get_page_size(void)
{
	static DWORD page_size;
	SYSTEM_INFO sys;

	if (!page_size) {
		GetSystemInfo(&sys);
		page_size = sys.dwAllocationGranularity;
	}

	return page_size;
}

long git_mmap_pagesize(void)
{
	return get_page_size();
}

int p_mmap(git_map *out, size_t len, int prot, int flags, int fd, git_off_t offset)
{
	HANDLE fh = (HANDLE)_get_osfhandle(fd);
	DWORD page_size = get_page_size();
	DWORD fmap_prot = 0;
	DWORD view_prot = 0;
	DWORD off_low = 0;
	DWORD off_hi = 0;
	DWORD end_low;
	DWORD end_hi;
	git_off_t end;

	GIT_MMAP_VALIDATE(out, len, prot, flags);

	out->data = NULL;
	out->len = 0;
	out->fmh = NULL;

	if (fh == INVALID_HANDLE_VALUE) {
		errno = EBADF;
		giterr_set(GITERR_OS, "Failed to mmap. Invalid handle value");
		return -1;
	}

	if ((offset % page_size) != 0) { /* offset must be multiple of page size */
		errno = EINVAL;
		giterr_set(GITERR_OS, "Failed to mmap. Offset must be multiple of page size");
		return -1;
	}

	if (prot & GIT_PROT_WRITE)
		fmap_prot |= PAGE_READWRITE;
	else if (prot & GIT_PROT_READ)
		fmap_prot |= PAGE_READONLY;

	if (prot & GIT_PROT_WRITE)
		view_prot |= FILE_MAP_WRITE;
	if (prot & GIT_PROT_READ)
		view_prot |= FILE_MAP_READ;

	assert(sizeof(git_off_t) == 8);

	end = offset + len;
	end_low = (DWORD) end;
	end_hi = (DWORD) (end >> 32);

	out->fmh = CreateFileMapping(fh, NULL, fmap_prot, end_hi, end_low, NULL);
	if (!out->fmh || out->fmh == INVALID_HANDLE_VALUE) {
		giterr_set(GITERR_OS, "Failed to mmap. Invalid handle value");
		out->fmh = NULL;
		return -1;
	}

	off_low = (DWORD) offset;
	off_hi = (DWORD) (offset >> 32);

	out->data = MapViewOfFile(out->fmh, view_prot, off_hi, off_low, len);
	if (!out->data) {
		giterr_set(GITERR_OS, "Failed to mmap. No data written");
		CloseHandle(out->fmh);
		out->fmh = NULL;
		return -1;
	}
	out->len = len;

	return 0;
}

int p_munmap(git_map *map)
{
	int error = 0;

	assert(map != NULL);

	if (map->data) {
		if (!UnmapViewOfFile(map->data)) {
			giterr_set(GITERR_OS, "Failed to munmap. Could not unmap view of file");
			error = -1;
		}
		map->data = NULL;
	}

	if (map->fmh) {
		if (!CloseHandle(map->fmh)) {
			giterr_set(GITERR_OS, "Failed to munmap. Could not close handle");
			error = -1;
		}
		map->fmh = NULL;
	}

	return error;
}

#endif
