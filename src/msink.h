/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_msink_h__
#define INCLUDE_msink_h__

#include "common.h"
#include "posix.h"

#ifndef NO_MMAP

# include "map.h"

typedef struct git_msink_pos {
	size_t cursor;
	git_off_t offset;
} git_msink_pos;

#endif /* !NO_MMAP */

typedef struct git_msink_file {
	git_file fd; /* 0 is not initialized, and it is stdin anyway */
#ifndef NO_MMAP
	git_map block;
	git_off_t file_length;
	git_msink_pos pos;
#endif
} git_msink_file;

#ifndef NO_MMAP

extern bool git_msink_init(git_msink_file *msf, int fd);
extern git_off_t git_msink_seek(git_msink_file *msf, git_off_t offset, int whence);
extern int git_msink_write(git_msink_file *msf, const void *data, size_t size);
extern int git_msink_unmap(git_msink_file *msf);
extern bool git_msink_would_unmap(git_msink_file *msf, git_off_t offset);
extern bool git_msink_truncate(git_msink_file *msf);

#else

GIT_INLINE(bool) git_msink_init(git_msink_file *msf, int fd)
{
	if (0 >= fd)
		return false;
	msf->fd = fd;
	return true;
}

GIT_INLINE(git_off_t) git_msink_seek(git_msink_file *msf, git_off_t offset, int whence)
{
	return p_lseek(msf->fd, offset, whence);
}

GIT_INLINE(int) git_msink_write(git_msink_file *msf, const void *blob, size_t size)
{
	return p_write(msf->fd, blob, size);
}

GIT_INLINE(int) git_msink_unmap(git_msink_file *msf)
{
	GIT_UNUSED(msf);
	return 0;
}

GIT_INLINE(bool) git_msink_would_unmap(git_msink_file *msf, git_off_t offset)
{
	GIT_UNUSED(msf);
	GIT_UNUSED(offset);
	return false;
}

GIT_INLINE(bool) git_msink_truncate(git_msink_file *msf)
{
	GIT_UNUSED(msf);
	return true;
}

#endif

GIT_INLINE(int) git_msink_close(git_msink_file *msf)
{
	int err;
	if (0 >= msf->fd)
		err = 0;
	else {
		err = p_close(msf->fd);
		msf->fd = -1;
	}
	return err;
}


#endif
