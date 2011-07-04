
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

int p_mmap(git_map *out, size_t len, int prot, int flags, int fd, git_off_t offset)
{
	HANDLE fh = (HANDLE)_get_osfhandle(fd);
	DWORD page_size = get_page_size();
	DWORD fmap_prot = 0;
	DWORD view_prot = 0;
	DWORD off_low = 0;
	DWORD off_hi  = 0;
	git_off_t page_start;
	git_off_t page_offset;

	assert((out != NULL) && (len > 0));

	if ((out == NULL) || (len == 0)) {
		errno = EINVAL;
		return git__throw(GIT_ERROR, "Failed to mmap. No map or zero length");
	}

	out->data = NULL;
	out->len = 0;
	out->fmh = NULL;

	if (fh == INVALID_HANDLE_VALUE) {
		errno = EBADF;
		return git__throw(GIT_ERROR, "Failed to mmap. Invalid handle value");
	}

	if (prot & GIT_PROT_WRITE)
		fmap_prot |= PAGE_READWRITE;
	else if (prot & GIT_PROT_READ)
		fmap_prot |= PAGE_READONLY;
	else {
		errno = EINVAL;
		return git__throw(GIT_ERROR, "Failed to mmap. Invalid protection parameters");
	}

	if (prot & GIT_PROT_WRITE)
		view_prot |= FILE_MAP_WRITE;
	if (prot & GIT_PROT_READ)
		view_prot |= FILE_MAP_READ;

	if (flags & GIT_MAP_FIXED) {
		errno = EINVAL;
		return git__throw(GIT_ERROR, "Failed to mmap. FIXED not set");
	}

	page_start = (offset / page_size) * page_size;
	page_offset = offset - page_start;

	if (page_offset != 0) {  /* offset must be multiple of page size */
		errno = EINVAL;
		return git__throw(GIT_ERROR, "Failed to mmap. Offset must be multiple of page size");
	}

	out->fmh = CreateFileMapping(fh, NULL, fmap_prot, 0, 0, NULL);
	if (!out->fmh || out->fmh == INVALID_HANDLE_VALUE) {
		/* errno = ? */
		out->fmh = NULL;
		return git__throw(GIT_ERROR, "Failed to mmap. Invalid handle value");
	}

	assert(sizeof(git_off_t) == 8);
	off_low = (DWORD)(page_start);
	off_hi = (DWORD)(page_start >> 32);
	out->data = MapViewOfFile(out->fmh, view_prot, off_hi, off_low, len);
	if (!out->data) {
		/* errno = ? */
		CloseHandle(out->fmh);
		out->fmh = NULL;
		return git__throw(GIT_ERROR, "Failed to mmap. No data written");
	}
	out->len = len;

	return GIT_SUCCESS;
}

int p_munmap(git_map *map)
{
	assert(map != NULL);

	if (!map)
		return git__throw(GIT_ERROR, "Failed to munmap. Map does not exist");

	if (map->data) {
		if (!UnmapViewOfFile(map->data)) {
			/* errno = ? */
			CloseHandle(map->fmh);
			map->data = NULL;
			map->fmh = NULL;
			return git__throw(GIT_ERROR, "Failed to munmap. Could not unmap view of file");
		}
		map->data = NULL;
	}

	if (map->fmh) {
		if (!CloseHandle(map->fmh)) {
			/* errno = ? */
			map->fmh = NULL;
			return git__throw(GIT_ERROR, "Failed to munmap. Could not close handle");
		}
		map->fmh = NULL;
	}

	return GIT_SUCCESS;
}


