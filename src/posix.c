/*
 * Copyright (C) 2009-2012 the libgit2 contributors
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

int p_open(const char *path, int flags, ...)
{
	mode_t mode = 0;

	if (flags & O_CREAT)
	{
		va_list arg_list;

		va_start(arg_list, flags);
		mode = (mode_t)va_arg(arg_list, int);
		va_end(arg_list);
	}

	return open(path, flags | O_BINARY, mode);
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
		return -1;

	git_path_mkposix(buffer_out);
	git_path_string_to_dir(buffer_out, size); /* append trailing slash */

	return 0;
}

int p_rename(const char *from, const char *to)
{
	if (!link(from, to)) {
		p_unlink(from);
		return 0;
	}

	if (!rename(from, to))
		return 0;

	return -1;
}

#endif

int p_read(git_file fd, void *buf, size_t cnt)
{
	char *b = buf;
	while (cnt) {
		ssize_t r;
#ifdef GIT_WIN32
		assert((size_t)((unsigned int)cnt) == cnt);
		r = read(fd, b, (unsigned int)cnt);
#else
		r = read(fd, b, cnt);
#endif
		if (r < 0) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
			return -1;
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
		ssize_t r;
#ifdef GIT_WIN32
		assert((size_t)((unsigned int)cnt) == cnt);
		r = write(fd, b, (unsigned int)cnt);
#else
		r = write(fd, b, cnt);
#endif
		if (r < 0) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
			return -1;
		}
		if (!r) {
			errno = EPIPE;
			return -1;
		}
		cnt -= r;
		b += r;
	}
	return 0;
}
