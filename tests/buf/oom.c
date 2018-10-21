#include "clar_libgit2.h"
#include "buffer.h"

/*
 * We want to use some ridiculous size that `malloc` will fail with
 * but that does not otherwise interfere with testing.  On Linux, choose
 * a number that is large enough to fail immediately but small enough
 * that valgrind doesn't believe it to erroneously be a negative number.
 * On macOS, choose a number that is large enough to fail immediately
 * without having libc print warnings to stderr.
 */
#if defined(GIT_ARCH_64) && defined(__linux__)
# define TOOBIG 0x0fffffffffffffff
#elif defined(GIT_ARCH_64)
# define TOOBIG 0xffffffffffffff00
#endif

/**
 * If we make a ridiculously large request the first time we
 * actually allocate some space in the git_buf, the realloc()
 * will fail.  And because the git_buf_grow() wrapper always
 * sets mark_oom, the code in git_buf_try_grow() will free
 * the internal buffer and set it to git_buf__oom.
 *
 * We initialized the internal buffer to (the static variable)
 * git_buf__initbuf.  The purpose of this test is to make sure
 * that we don't try to free the static buffer.
 *
 * Skip this test entirely on 32-bit platforms; a buffer large enough
 * to guarantee malloc failures is so large that valgrind considers
 * it likely to be an error.
 */
void test_buf_oom__grow(void)
{
#ifdef GIT_ARCH_64
	git_buf buf = GIT_BUF_INIT;

	git_buf_clear(&buf);

	cl_assert(git_buf_grow(&buf, TOOBIG) == -1);
	cl_assert(git_buf_oom(&buf));

	git_buf_dispose(&buf);
#else
    cl_skip();
#endif
}

void test_buf_oom__grow_by(void)
{
	git_buf buf = GIT_BUF_INIT;

	buf.size = SIZE_MAX-10;

	cl_assert(git_buf_grow_by(&buf, 50) == -1);
	cl_assert(git_buf_oom(&buf));
}
