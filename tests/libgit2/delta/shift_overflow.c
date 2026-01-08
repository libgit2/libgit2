#include "clar_libgit2.h"
#include "delta.h"

void test_delta_shift_overflow__hdr_sz_shift_limit(void)
{
	unsigned char base[16] = { 0 };
	unsigned char delta[] = {
		0x80, 0x80, 0x80, 0x80, 0x80,
		0x80, 0x80, 0x80, 0x80,
		0x80, 0x01
	};
	void *out;
	size_t outlen;

	cl_git_fail(git_delta_apply(&out, &outlen, base, sizeof(base), delta, sizeof(delta)));
}
