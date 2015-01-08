#include "clar_libgit2.h"
#include "buffer.h"

#if defined(GIT_ARCH_64)
# define TOOBIG 0xffffffffffffff00
#else
# define TOOBIG 0xffffff00
#endif

void test_buf_oom__grow(void)
{
	git_buf buf = GIT_BUF_INIT;

	git_buf_clear(&buf);

	cl_assert( git_buf_grow(&buf, TOOBIG) == -1 );
	cl_assert( git_buf_oom(&buf) );

	git_buf_free(&buf);
}
