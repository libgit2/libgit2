#include "clay_libgit2.h"

/* compare prefixes */
void test_string__0(void)
{
	must_be_true(git__prefixcmp("", "") == 0);
	must_be_true(git__prefixcmp("a", "") == 0);
	must_be_true(git__prefixcmp("", "a") < 0);
	must_be_true(git__prefixcmp("a", "b") < 0);
	must_be_true(git__prefixcmp("b", "a") > 0);
	must_be_true(git__prefixcmp("ab", "a") == 0);
	must_be_true(git__prefixcmp("ab", "ac") < 0);
	must_be_true(git__prefixcmp("ab", "aa") > 0);
}

/* compare suffixes */
void test_string__1(void)
{
	must_be_true(git__suffixcmp("", "") == 0);
	must_be_true(git__suffixcmp("a", "") == 0);
	must_be_true(git__suffixcmp("", "a") < 0);
	must_be_true(git__suffixcmp("a", "b") < 0);
	must_be_true(git__suffixcmp("b", "a") > 0);
	must_be_true(git__suffixcmp("ba", "a") == 0);
	must_be_true(git__suffixcmp("zaa", "ac") < 0);
	must_be_true(git__suffixcmp("zaz", "ac") > 0);
}

