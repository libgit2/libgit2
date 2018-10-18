#include "clar_libgit2.h"

void test_core_strtol__int32(void)
{
	int32_t i;

	cl_git_pass(git__strtol32(&i, "123", NULL, 10));
	cl_assert(i == 123);
	cl_git_pass(git__strtol32(&i, "  +123 ", NULL, 10));
	cl_assert(i == 123);
	cl_git_pass(git__strtol32(&i, "  +2147483647 ", NULL, 10));
	cl_assert(i == 2147483647);
	cl_git_pass(git__strtol32(&i, "  -2147483648 ", NULL, 10));
	cl_assert(i == -2147483648LL);
	
	cl_git_fail(git__strtol32(&i, "  2147483657 ", NULL, 10));
	cl_git_fail(git__strtol32(&i, "  -2147483657 ", NULL, 10));
}

static void assert_l64_parses(const char *string, int64_t expected, int base)
{
	int64_t i;
	cl_git_pass(git__strntol64(&i, string, strlen(string), NULL, base));
	cl_assert_equal_i(i, expected);
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
