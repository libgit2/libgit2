#define GIT__WIN32_NO_HIDE_FILEOPS
#include "fileops.h"

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

