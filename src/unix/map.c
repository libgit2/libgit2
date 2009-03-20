
#include "map.h"
#include <sys/mman.h>
#include <errno.h>


int git__mmap(git_map *out, size_t len, int prot, int flags, int fd, off_t offset)
{
	int mprot = 0;
	int mflag = 0;

	assert((out != NULL) && (len > 0));

	if ((out == NULL) || (len == 0)) {
		errno = EINVAL;
		return GIT_ERROR;
	}

	out->data = NULL;
	out->len = 0;

	if (prot & GIT_PROT_WRITE)
		mprot = PROT_WRITE;
	else if (prot & GIT_PROT_READ)
		mprot = PROT_READ;
	else {
		errno = EINVAL;
		return GIT_ERROR;
	}

	if ((flags & GIT_MAP_TYPE) == GIT_MAP_SHARED)
		mflag = MAP_SHARED;
	else if ((flags & GIT_MAP_TYPE) == GIT_MAP_PRIVATE)
		mflag = MAP_PRIVATE;

	if (flags & GIT_MAP_FIXED) {
		errno = EINVAL;
		return GIT_ERROR;
	}

	out->data = mmap(NULL, len, mprot, mflag, fd, offset);
	if (!out->data || out->data == MAP_FAILED)
		return git_os_error();
	out->len = len;

	return GIT_SUCCESS;
}

int git__munmap(git_map *map)
{
	assert(map != NULL);

	if (!map)
		return GIT_ERROR;

	munmap(map->data, map->len);

	return GIT_SUCCESS;
}


