#include "clar_libgit2.h"
#include "varint.h"

void test_encoding__decode(void)
{
	const unsigned char *buf = (unsigned char *)"AB";
	size_t size;

	cl_assert(git_decode_varint(buf, &size) == 65);
	cl_assert(size == 1);

	buf = (unsigned char *)"\xfe\xdc\xbaXY";
	cl_assert(git_decode_varint(buf, &size) == 267869656);
	cl_assert(size == 4);

	buf = (unsigned char *)"\xaa\xaa\xfe\xdc\xbaXY";
	cl_assert(git_decode_varint(buf, &size) == UINT64_C(1489279344088));
	cl_assert(size == 6);

	buf = (unsigned char *)"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xfe\xdc\xbaXY";
	cl_assert(git_decode_varint(buf, &size) == 0);
	cl_assert(size == 0);

}

void test_encoding__encode(void)
{
	unsigned char buf[100];
	cl_assert(git_encode_varint(buf, 100, 65) == 1);
	cl_assert(buf[0] == 'A');

	cl_assert(git_encode_varint(buf, 1, 1) == 1);
	cl_assert(!memcmp(buf, "\x01", 1));

	cl_assert(git_encode_varint(buf, 100, 267869656) == 4);
	cl_assert(!memcmp(buf, "\xfe\xdc\xbaX", 4));

	cl_assert(git_encode_varint(buf, 100, UINT64_C(1489279344088)) == 6);
	cl_assert(!memcmp(buf, "\xaa\xaa\xfe\xdc\xbaX", 6));

	cl_assert(git_encode_varint(buf, 1, UINT64_C(1489279344088)) == -1);
}

void test_encoding__get_be32_be64(void)
{
	unsigned char buf[16];
	size_t i;

	/* the readers must work at any offset, aligned or not */
	for (i = 0; i < 8; i++) {
		memset(buf, 0, sizeof(buf));
		memcpy(buf + i, "\x01\x02\x03\x04\x05\x06\x07\x08", 8);
		cl_assert_equal_i(git__get_be32(buf + i), 0x01020304);
		cl_assert_equal_i(git__get_be32(buf + i + 4), 0x05060708);
		cl_assert(git__get_be64(buf + i) == UINT64_C(0x0102030405060708));

		memcpy(buf + i, "\xff\xfe\xfd\xfc\x80\x00\x00\x01", 8);
		cl_assert(git__get_be32(buf + i) == 0xfffefdfc);
		cl_assert(git__get_be32(buf + i + 4) == 0x80000001);
		cl_assert(git__get_be64(buf + i) == UINT64_C(0xfffefdfc80000001));

		memset(buf + i, 0xff, 8);
		cl_assert(git__get_be32(buf + i) == UINT32_MAX);
		cl_assert(git__get_be64(buf + i) == UINT64_MAX);
	}
}
