#include "clar_libgit2.h"

/* compare prefixes */
void test_core_string__0(void)
{
	cl_assert(git__prefixcmp("", "") == 0);
	cl_assert(git__prefixcmp("a", "") == 0);
	cl_assert(git__prefixcmp("", "a") < 0);
	cl_assert(git__prefixcmp("a", "b") < 0);
	cl_assert(git__prefixcmp("b", "a") > 0);
	cl_assert(git__prefixcmp("ab", "a") == 0);
	cl_assert(git__prefixcmp("ab", "ac") < 0);
	cl_assert(git__prefixcmp("ab", "aa") > 0);
}

/* compare suffixes */
void test_core_string__1(void)
{
	cl_assert(git__suffixcmp("", "") == 0);
	cl_assert(git__suffixcmp("a", "") == 0);
	cl_assert(git__suffixcmp("", "a") < 0);
	cl_assert(git__suffixcmp("a", "b") < 0);
	cl_assert(git__suffixcmp("b", "a") > 0);
	cl_assert(git__suffixcmp("ba", "a") == 0);
	cl_assert(git__suffixcmp("zaa", "ac") < 0);
	cl_assert(git__suffixcmp("zaz", "ac") > 0);
}

/* compare icase sorting with case equality */
void test_core_string__2(void)
{
	cl_assert(git__strcasesort_cmp("", "") == 0);
	cl_assert(git__strcasesort_cmp("foo", "foo") == 0);
	cl_assert(git__strcasesort_cmp("foo", "bar") > 0);
	cl_assert(git__strcasesort_cmp("bar", "foo") < 0);
	cl_assert(git__strcasesort_cmp("foo", "FOO") > 0);
	cl_assert(git__strcasesort_cmp("FOO", "foo") < 0);
	cl_assert(git__strcasesort_cmp("foo", "BAR") > 0);
	cl_assert(git__strcasesort_cmp("BAR", "foo") < 0);
	cl_assert(git__strcasesort_cmp("fooBar", "foobar") < 0);
}

void test_core_string__strcmp(void)
{
	cl_assert(git__strcmp("", "") == 0);
	cl_assert(git__strcmp("foo", "foo") == 0);
	cl_assert(git__strcmp("Foo", "foo") < 0);
	cl_assert(git__strcmp("foo", "FOO") > 0);
	cl_assert(git__strcmp("foo", "fOO") > 0);

	cl_assert(strcmp("rt\303\202of", "rt dev\302\266h") > 0);
	cl_assert(strcmp("e\342\202\254ghi=", "et") > 0);
	cl_assert(strcmp("rt dev\302\266h", "rt\303\202of") < 0);
	cl_assert(strcmp("et", "e\342\202\254ghi=") < 0);
	cl_assert(strcmp("\303\215", "\303\255") < 0);

	cl_assert(git__strcmp("rt\303\202of", "rt dev\302\266h") > 0);
	cl_assert(git__strcmp("e\342\202\254ghi=", "et") > 0);
	cl_assert(git__strcmp("rt dev\302\266h", "rt\303\202of") < 0);
	cl_assert(git__strcmp("et", "e\342\202\254ghi=") < 0);
	cl_assert(git__strcmp("\303\215", "\303\255") < 0);
}

void test_core_string__strcasecmp(void)
{
	cl_assert(git__strcasecmp("", "") == 0);
	cl_assert(git__strcasecmp("foo", "foo") == 0);
	cl_assert(git__strcasecmp("foo", "Foo") == 0);
	cl_assert(git__strcasecmp("foo", "FOO") == 0);
	cl_assert(git__strcasecmp("foo", "fOO") == 0);

	cl_assert(strcasecmp("rt\303\202of", "rt dev\302\266h") > 0);
	cl_assert(strcasecmp("e\342\202\254ghi=", "et") > 0);
	cl_assert(strcasecmp("rt dev\302\266h", "rt\303\202of") < 0);
	cl_assert(strcasecmp("et", "e\342\202\254ghi=") < 0);
	cl_assert(strcasecmp("\303\215", "\303\255") < 0);

	cl_assert(git__strcasecmp("rt\303\202of", "rt dev\302\266h") > 0);
	cl_assert(git__strcasecmp("e\342\202\254ghi=", "et") > 0);
	cl_assert(git__strcasecmp("rt dev\302\266h", "rt\303\202of") < 0);
	cl_assert(git__strcasecmp("et", "e\342\202\254ghi=") < 0);
	cl_assert(git__strcasecmp("\303\215", "\303\255") < 0);
}

#ifdef GIT_WIN32
void test_core_string__unicode(void)
{
	wchar_t *wsrc1 = L"这";
	wchar_t *wsrc2 = L"StorageNewsletter » Gartner Ranks Top Seven Enterprise Endpoint Backup Products.pdf";
	wchar_t *wdest1, *wdest2;
	char *ndest1, *ndest2;
	cl_assert(git__utf16_to_8_alloc(&ndest1, wsrc1));
	cl_assert(git__utf16_to_8_alloc(&ndest2, wsrc2));
	cl_assert(git__utf8_to_16_alloc(&wdest1, ndest1));
	cl_assert(git__utf8_to_16_alloc(&wdest2, ndest2));
	cl_assert(wcscmp(wsrc1, wdest1) == 0);
	cl_assert(wcscmp(wsrc2, wdest2) == 0);
	git__free(ndest1);
	git__free(ndest2);
	git__free(wdest1);
	git__free(wdest2);
}
#endif
