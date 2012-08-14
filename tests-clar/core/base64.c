#include "clar_libgit2.h"
#include "base64.h"

#define BUF_SIZE 16

void test_core_base64__encode(void)
{
	char b64[BUF_SIZE];

	cl_git_pass(git_base64_encode(b64, BUF_SIZE, "libgit2", 7));
	cl_assert(memcmp(b64, "bGliZ2l0Mg==\0", 13) == 0);

	cl_git_pass(git_base64_encode(b64, BUF_SIZE, "libgit2libgit2libgit2", 21));
	cl_assert(memcmp(b64, "bGliZ2l0MmxpYmdp", BUF_SIZE) == 0);
}
