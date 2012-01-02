#include "clay_libgit2.h"
#include "posix.h"

void clay_on_init(void)
{
	git_threads_init();
}

void clay_on_shutdown(void)
{
	git_threads_shutdown();
}

void cl_git_mkfile(const char *filename, const char *content)
{
	int fd;

	fd = p_creat(filename, 0666);
	cl_assert(fd != 0);

	if (content) {
		cl_must_pass(p_write(fd, content, strlen(content)));
	} else {
		cl_must_pass(p_write(fd, filename, strlen(filename)));
		cl_must_pass(p_write(fd, "\n", 1));
	}

	cl_must_pass(p_close(fd));
}
