#include "clar_libgit2.h"
#include "posix.h"

void clar_on_init(void)
{
	git_threads_init();
}

void clar_on_shutdown(void)
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

void cl_git_append2file(const char *filename, const char *new_content)
{
	int fd = p_open(filename, O_WRONLY | O_APPEND | O_CREAT);
	cl_assert(fd != 0);
	if (!new_content)
		new_content = "\n";
	cl_must_pass(p_write(fd, new_content, strlen(new_content)));
	cl_must_pass(p_close(fd));
	cl_must_pass(p_chmod(filename, 0644));
}

