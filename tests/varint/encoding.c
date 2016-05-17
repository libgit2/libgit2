#include "clar_libgit2.h"
#include "varint.h"

void test_varint_encoding__decode(void)
{
	const unsigned char *buf = (unsigned char *)"AB";
	const unsigned char *orig_buf = buf;
	cl_assert(decode_varint(&buf) == 65);
	cl_assert(buf == orig_buf + 1);

	buf = (unsigned char *)"\xfe\xdc\xbaXY";
	orig_buf = buf;
	cl_assert(decode_varint(&buf) == 267869656);
	cl_assert(buf == orig_buf + 4);

	buf = (unsigned char *)"\xaa\xaa\xfe\xdc\xbaXY";
	orig_buf = buf;
	cl_assert(decode_varint(&buf) == 1489279344088ULL);
	cl_assert(buf == orig_buf + 6);

}

void test_varint_encoding__encode(void)
{
	unsigned char buf[100];
	cl_assert(encode_varint(65, buf) == 1);
	cl_assert(buf[0] == 'A');

	cl_assert(encode_varint(267869656, buf) == 4);
	cl_assert(!memcmp(buf, "\xfe\xdc\xbaX", 4));

	cl_assert(encode_varint(1489279344088ULL, buf) == 6);
	cl_assert(!memcmp(buf, "\xaa\xaa\xfe\xdc\xbaX", 6));
}
