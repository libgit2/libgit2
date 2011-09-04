#include "clay_libgit2.h"
#include "filebuf.h"

/* make sure git_filebuf_open doesn't delete an existing lock */
void test_filebuf__0(void)
{
	git_filebuf file;
	int fd;
	char test[] = "test", testlock[] = "test.lock";

	fd = p_creat(testlock, 0744);
	must_pass(fd);
	must_pass(p_close(fd));
	must_fail(git_filebuf_open(&file, test, 0));
	must_pass(git_futils_exists(testlock));
	must_pass(p_unlink(testlock));
}


/* make sure GIT_FILEBUF_APPEND works as expected */
void test_filebuf__1(void)
{
	git_filebuf file;
	int fd;
	char test[] = "test";

	fd = p_creat(test, 0644);
	must_pass(fd);
	must_pass(p_write(fd, "libgit2 rocks\n", 14));
	must_pass(p_close(fd));

	must_pass(git_filebuf_open(&file, test, GIT_FILEBUF_APPEND));
	must_pass(git_filebuf_printf(&file, "%s\n", "libgit2 rocks"));
	must_pass(git_filebuf_commit(&file));

	must_pass(p_unlink(test));
}


/* make sure git_filebuf_write writes large buffer correctly */
void test_filebuf__2(void)
{
	git_filebuf file;
	char test[] = "test";
	unsigned char buf[4096 * 4]; /* 2 * WRITE_BUFFER_SIZE */

	memset(buf, 0xfe, sizeof(buf));
	must_pass(git_filebuf_open(&file, test, 0));
	must_pass(git_filebuf_write(&file, buf, sizeof(buf)));
	must_pass(git_filebuf_commit(&file));

	must_pass(p_unlink(test));
}

