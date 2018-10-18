#include "clar_libgit2.h"

static void assert_l32_parses(const char *string, int32_t expected, int base)
{
	int32_t i;
	cl_git_pass(git__strntol32(&i, string, strlen(string), NULL, base));
	cl_assert_equal_i(i, expected);
}

static void assert_l32_fails(const char *string, int base)
{
	int32_t i;
	cl_git_fail(git__strntol32(&i, string, strlen(string), NULL, base));
}

static void assert_l64_parses(const char *string, int64_t expected, int base)
{
	int64_t i;
	cl_git_pass(git__strntol64(&i, string, strlen(string), NULL, base));
	cl_assert_equal_i(i, expected);
}

void test_core_strtol__int32(void)
{
	assert_l32_parses("123", 123, 10);
	assert_l32_parses("  +123 ", 123, 10);
	assert_l32_parses("  +2147483647 ", 2147483647, 10);
	assert_l32_parses("  -2147483648 ", -2147483648LL, 10);

	assert_l32_fails("  2147483657 ", 10);
	assert_l32_fails("  -2147483657 ", 10);
}

void test_core_strtol__int64(void)
{
	assert_l64_parses("123", 123, 10);
	assert_l64_parses("  +123 ", 123, 10);
	assert_l64_parses("  +2147483647 ", 2147483647, 10);
	assert_l64_parses("  -2147483648 ", -2147483648LL, 10);
	assert_l64_parses("  2147483657 ", 2147483657LL, 10);
	assert_l64_parses("  -2147483657 ", -2147483657LL, 10);
	assert_l64_parses(" 9223372036854775807  ", INT64_MAX, 10);
	assert_l64_parses("   -9223372036854775808  ", INT64_MIN, 10);
	assert_l64_parses("   0x7fffffffffffffff  ", INT64_MAX, 16);
	assert_l64_parses("   -0x8000000000000000   ", INT64_MIN, 16);
}
