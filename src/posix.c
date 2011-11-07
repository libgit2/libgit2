/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#include "common.h"
#include "posix.h"
#include "path.h"
#include <stdio.h>
#include <ctype.h>

#ifndef GIT_WIN32

int p_open(const char *path, int flags)
{
	return open(path, flags | O_BINARY);
}

int p_creat(const char *path, mode_t mode)
{
	return open(path, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, mode);
}

int p_getcwd(char *buffer_out, size_t size)
{
	char *cwd_buffer;

	assert(buffer_out && size > 0);

	cwd_buffer = getcwd(buffer_out, size);

	if (cwd_buffer == NULL)
		return git__throw(GIT_EOSERR, "Failed to retrieve current working directory");

	git_path_mkposix(buffer_out);

	git_path_join(buffer_out, buffer_out, "");	//Ensure the path ends with a trailing slash
	return GIT_SUCCESS;
}

int p_rename(const char *from, const char *to)
{
	if (!link(from, to)) {
		p_unlink(from);
		return GIT_SUCCESS;
	}

	if (!rename(from, to))
		return GIT_SUCCESS;

	return GIT_ERROR;

}

#endif

int p_read(git_file fd, void *buf, size_t cnt)
{
	char *b = buf;
	while (cnt) {
		ssize_t r = read(fd, b, cnt);
		if (r < 0) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
			return GIT_EOSERR;
		}
		if (!r)
			break;
		cnt -= r;
		b += r;
	}
	return (int)(b - (char *)buf);
}

int p_write(git_file fd, const void *buf, size_t cnt)
{
	const char *b = buf;
	while (cnt) {
		ssize_t r = write(fd, b, cnt);
		if (r < 0) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
			return GIT_EOSERR;
		}
		if (!r) {
			errno = EPIPE;
			return GIT_EOSERR;
		}
		cnt -= r;
		b += r;
	}
	return GIT_SUCCESS;
}
