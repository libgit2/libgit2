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

void test_delta_shift_overflow__hdr_sz_base_size_bits_lost(void)
{
	unsigned char base[16] = { 0 };
	/*
	 * A base size of 16, followed by continuation bytes so that the
	 * last byte of the size only has bits beyond the size_t range.
	 */
	unsigned char delta[] = {
		0x90, 0x80, 0x80, 0x80, 0x80,
		0x80, 0x80, 0x80, 0x80, 0x02,
		0x00
	};
	void *out;
	size_t outlen;

	cl_git_fail(git_delta_apply(&out, &outlen, base, sizeof(base), delta, sizeof(delta)));
}

void test_delta_shift_overflow__hdr_sz_result_size_bits_lost(void)
{
	unsigned char base[16] = { 0 };
	/*
	 * A result size of 0, followed by continuation bytes so that the
	 * last byte of the size only has bits beyond the size_t range.
	 */
	unsigned char delta[] = {
		0x10,
		0x80, 0x80, 0x80, 0x80, 0x80,
		0x80, 0x80, 0x80, 0x80, 0x02
	};
	void *out;
	size_t outlen;

	cl_git_fail(git_delta_apply(&out, &outlen, base, sizeof(base), delta, sizeof(delta)));
}
