#include <git2/common.h>

#ifndef GIT_WIN32

#include "map.h"
#include <sys/mman.h>
#include <errno.h>

int p_mmap(git_map *out, size_t len, int prot, int flags, int fd, git_off_t offset)
{
	int mprot = 0;
	int mflag = 0;

	assert((out != NULL) && (len > 0));

	if ((out == NULL) || (len == 0)) {
		errno = EINVAL;
		return git__throw(GIT_ERROR, "Failed to mmap. No map or zero length");
	}

	out->data = NULL;
	out->len = 0;

	if (prot & GIT_PROT_WRITE)
		mprot = PROT_WRITE;
	else if (prot & GIT_PROT_READ)
		mprot = PROT_READ;
	else {
		errno = EINVAL;
		return git__throw(GIT_ERROR, "Failed to mmap. Invalid protection parameters");
	}

	if ((flags & GIT_MAP_TYPE) == GIT_MAP_SHARED)
		mflag = MAP_SHARED;
	else if ((flags & GIT_MAP_TYPE) == GIT_MAP_PRIVATE)
		mflag = MAP_PRIVATE;

	if (flags & GIT_MAP_FIXED) {
		errno = EINVAL;
		return git__throw(GIT_ERROR, "Failed to mmap. FIXED not set");
	}

	out->data = mmap(NULL, len, mprot, mflag, fd, offset);
	if (!out->data || out->data == MAP_FAILED)
		return git__throw(GIT_EOSERR, "Failed to mmap. Could not write data");
	out->len = len;

	return GIT_SUCCESS;
}

int p_munmap(git_map *map)
{
	assert(map != NULL);

	if (!map)
		return git__throw(GIT_ERROR, "Failed to munmap. Map does not exist");

	munmap(map->data, map->len);

	return GIT_SUCCESS;
}

#endif

