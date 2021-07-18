#include <dlfcn.h>
#include <stdarg.h>
#include <fcntl.h>
#include <string.h>

#include "clar_libgit2.h"

int open(const char *path, volatile int flags, ...)
{
	int (*next_open)(const char *path, volatile int flags, ...);
	mode_t mode = 0;

	cl_assert_(
		strstr(path, "//") == NULL,
		"Path contains empty segment"
	);

	if (flags & O_CREAT) {
		va_list arg_list;

		va_start(arg_list, flags);
		mode = (mode_t)va_arg(arg_list, int);
		va_end(arg_list);
	}

	next_open = dlsym(RTLD_NEXT,"open");
	return next_open(path, flags, mode);
}
