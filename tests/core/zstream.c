#include "clar_libgit2.h"
#include "buffer.h"
#include "zstream.h"

static const char *data = "This is a test test test of This is a test";

#define INFLATE_EXTRA 2

static void assert_zlib_equal_(
	const void *expected, size_t e_len,
	const void *compressed, size_t c_len,
	const char *msg, const char *file, int line)
{
	z_stream stream;
	char *expanded = git__calloc(1, e_len + INFLATE_EXTRA);
	cl_assert(expanded);

	memset(&stream, 0, sizeof(stream));
	stream.next_out  = (Bytef *)expanded;
	stream.avail_out = (uInt)(e_len + INFLATE_EXTRA);
	stream.next_in   = (Bytef *)compressed;
	stream.avail_in  = (uInt)c_len;

	cl_assert(inflateInit(&stream) == Z_OK);
	cl_assert(inflate(&stream, Z_FINISH));
	inflateEnd(&stream);

	clar__assert_equal(
		file, line, msg, 1,
		"%d", (int)stream.total_out, (int)e_len);
	clar__assert_equal(
		file, line, "Buffer len was not exact match", 1,
		"%d", (int)stream.avail_out, (int)INFLATE_EXTRA);

	clar__assert(
		memcmp(expanded, expected, e_len) == 0,
		file, line, "uncompressed data did not match", NULL, 1);

	git__free(expanded);
}

#define assert_zlib_equal(E,EL,C,CL) \
	assert_zlib_equal_(E, EL, C, CL, #EL " != " #CL, __FILE__, (int)__LINE__)

void test_core_zstream__basic(void)
{
	git_zstream z = GIT_ZSTREAM_INIT;
	char out[128];
	size_t outlen = sizeof(out);

	cl_git_pass(git_zstream_init(&z));
	cl_git_pass(git_zstream_set_input(&z, data, strlen(data) + 1));
	cl_git_pass(git_zstream_get_output(out, &outlen, &z));
	cl_assert(git_zstream_done(&z));
	cl_assert(outlen > 0);
	git_zstream_free(&z);

	assert_zlib_equal(data, strlen(data) + 1, out, outlen);
}

void test_core_zstream__buffer(void)
{
	git_buf out = GIT_BUF_INIT;
	cl_git_pass(git_zstream_deflatebuf(&out, data, strlen(data) + 1));
	assert_zlib_equal(data, strlen(data) + 1, out.ptr, out.size);
	git_buf_free(&out);
}

#define BIG_STRING_PART "Big Data IS Big - Long Data IS Long - We need a buffer larger than 1024 x 1024 to make sure we trigger chunked compression - Big Big Data IS Bigger than Big - Long Long Data IS Longer than Long"

void test_core_zstream__big_data(void)
{
	git_buf in = GIT_BUF_INIT;
	git_buf out = GIT_BUF_INIT;
	size_t scan;

	/* make a big string that's easy to compress */
	while (in.size < 1024 * 1024)
		cl_git_pass(git_buf_put(&in, BIG_STRING_PART, strlen(BIG_STRING_PART)));

	cl_git_pass(git_zstream_deflatebuf(&out, in.ptr, in.size));
	assert_zlib_equal(in.ptr, in.size, out.ptr, out.size);

	git_buf_free(&out);

	/* make a big string that's hard to compress */

	srand(0xabad1dea);
	for (scan = 0; scan < in.size; ++scan)
		in.ptr[scan] = (char)rand();

	cl_git_pass(git_zstream_deflatebuf(&out, in.ptr, in.size));
	assert_zlib_equal(in.ptr, in.size, out.ptr, out.size);

	git_buf_free(&out);

	git_buf_free(&in);
}
