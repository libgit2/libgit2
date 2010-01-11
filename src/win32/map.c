
#include "map.h"
#include <errno.h>


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

int git__mmap(git_map *out, size_t len, int prot, int flags, int fd, off_t offset)
{
	HANDLE fh = (HANDLE)_get_osfhandle(fd);
	DWORD page_size = get_page_size();
	DWORD fmap_prot = 0;
	DWORD view_prot = 0;
	DWORD off_low = 0;
	DWORD off_hi  = 0;
	off_t page_start;
	off_t page_offset;

	assert((out != NULL) && (len > 0));

	if ((out == NULL) || (len == 0)) {
		errno = EINVAL;
		return GIT_ERROR;
	}

	out->data = NULL;
	out->len = 0;
	out->fmh = NULL;

	if (fh == INVALID_HANDLE_VALUE) {
		errno = EBADF;
		return GIT_ERROR;
	}

	if (prot & GIT_PROT_WRITE)
		fmap_prot |= PAGE_READWRITE;
	else if (prot & GIT_PROT_READ)
		fmap_prot |= PAGE_READONLY;
	else {
		errno = EINVAL;
		return GIT_ERROR;
	}

	if (prot & GIT_PROT_WRITE)
		view_prot |= FILE_MAP_WRITE;
	if (prot & GIT_PROT_READ)
		view_prot |= FILE_MAP_READ;

	if (flags & GIT_MAP_FIXED) {
		errno = EINVAL;
		return GIT_ERROR;
	}

	page_start = (offset / page_size) * page_size;
	page_offset = offset - page_start;

	if (page_offset != 0) {  /* offset must be multiple of page size */
		errno = EINVAL;
		return GIT_ERROR;
	}

	out->fmh = CreateFileMapping(fh, NULL, fmap_prot, 0, 0, NULL);
	if (!out->fmh || out->fmh == INVALID_HANDLE_VALUE) {
		/* errno = ? */
		out->fmh = NULL;
		return GIT_ERROR;
	}

	assert(sizeof(off_t) == 8);
	off_low = (DWORD)(page_start);
	off_hi = (DWORD)(page_start >> 32);
	out->data = MapViewOfFile(out->fmh, view_prot, off_hi, off_low, len);
	if (!out->data) {
		/* errno = ? */
		CloseHandle(out->fmh);
		out->fmh = NULL;
		return GIT_ERROR;
	}
	out->len = len;

	return GIT_SUCCESS;
}

int git__munmap(git_map *map)
{
	assert(map != NULL);

	if (!map)
		return GIT_ERROR;

	if (map->data) {
		if (!UnmapViewOfFile(map->data)) {
			/* errno = ? */
			CloseHandle(map->fmh);
			map->data = NULL;
			map->fmh = NULL;
			return GIT_ERROR;
		}
		map->data = NULL;
	}

	if (map->fmh) {
		if (!CloseHandle(map->fmh)) {
			/* errno = ? */
			map->fmh = NULL;
			return GIT_ERROR;
		}
		map->fmh = NULL;
	}

	return GIT_SUCCESS;
}


