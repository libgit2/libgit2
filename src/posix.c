#include "common.h"
#include "posix.h"
#include "path.h"
#include <stdio.h>
#include <ctype.h>

int p_open(const char *path, int flags)
{
	return open(path, flags | O_BINARY);
}

int p_creat(const char *path, int mode)
{
	return open(path, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, mode);
}

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

int p_write(git_file fd, void *buf, size_t cnt)
{
	char *b = buf;
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

int p_getcwd(char *buffer_out, size_t size)
{
	char *cwd_buffer;

	assert(buffer_out && size > 0);

#ifdef GIT_WIN32
	cwd_buffer = _getcwd(buffer_out, size);
#else
	cwd_buffer = getcwd(buffer_out, size);
#endif

	if (cwd_buffer == NULL)
		return git__throw(GIT_EOSERR, "Failed to retrieve current working directory");

	git_path_mkposix(buffer_out);

	git_path_join(buffer_out, buffer_out, "");	//Ensure the path ends with a trailing slash
	return GIT_SUCCESS;
}
