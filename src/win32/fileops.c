#define GIT__WIN32_NO_HIDE_FILEOPS
#include "fileops.h"
#include <errno.h>

int git__unlink(const char *path)
{
	chmod(path, 0666);
	return unlink(path);
}

int git__mkstemp(char *template)
{
	char *file = mktemp(template);
	if (file == NULL)
		return -1;
	return open(file, O_RDWR | O_CREAT | O_BINARY, 0600);
}

int git__fsync(int fd)
{
	HANDLE fh = (HANDLE)_get_osfhandle(fd);

	if (fh == INVALID_HANDLE_VALUE) {
		errno = EBADF;
		return -1;
	}

	if (!FlushFileBuffers(fh)) {
		DWORD code = GetLastError();

		if (code == ERROR_INVALID_HANDLE)
			errno = EINVAL;
		else
			errno = EIO;

		return -1;
	}

	return 0;
}

