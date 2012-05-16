#include "clar_libgit2.h"
#include "buffer.h"

#define TESTSTR "Have you seen that? Have you seeeen that??"
const char *test_string = TESTSTR;
const char *test_string_x2 = TESTSTR TESTSTR;

#define TESTSTR_4096 REP1024("1234")
#define TESTSTR_8192 REP1024("12341234")
const char *test_4096 = TESTSTR_4096;
const char *test_8192 = TESTSTR_8192;

/* test basic data concatenation */
void test_core_buffer__0(void)
{
	git_buf buf = GIT_BUF_INIT;

	cl_assert(buf.size == 0);

	git_buf_puts(&buf, test_string);
	cl_assert(git_buf_oom(&buf) == 0);
	cl_assert_equal_s(test_string, git_buf_cstr(&buf));

	git_buf_puts(&buf, test_string);
	cl_assert(git_buf_oom(&buf) == 0);
	cl_assert_equal_s(test_string_x2, git_buf_cstr(&buf));

	git_buf_free(&buf);
}

/* test git_buf_printf */
void test_core_buffer__1(void)
{
	git_buf buf = GIT_BUF_INIT;

	git_buf_printf(&buf, "%s %s %d ", "shoop", "da", 23);
	cl_assert(git_buf_oom(&buf) == 0);
	cl_assert_equal_s("shoop da 23 ", git_buf_cstr(&buf));

	git_buf_printf(&buf, "%s %d", "woop", 42);
	cl_assert(git_buf_oom(&buf) == 0);
	cl_assert_equal_s("shoop da 23 woop 42", git_buf_cstr(&buf));

	git_buf_free(&buf);
}

/* more thorough test of concatenation options */
void test_core_buffer__2(void)
{
	git_buf buf = GIT_BUF_INIT;
	int i;
	char data[128];

	cl_assert(buf.size == 0);

	/* this must be safe to do */
	git_buf_free(&buf);
	cl_assert(buf.size == 0);
	cl_assert(buf.asize == 0);

	/* empty buffer should be empty string */
	cl_assert_equal_s("", git_buf_cstr(&buf));
	cl_assert(buf.size == 0);
	/* cl_assert(buf.asize == 0); -- should not assume what git_buf does */

	/* free should set us back to the beginning */
	git_buf_free(&buf);
	cl_assert(buf.size == 0);
	cl_assert(buf.asize == 0);

	/* add letter */
	git_buf_putc(&buf, '+');
	cl_assert(git_buf_oom(&buf) == 0);
	cl_assert_equal_s("+", git_buf_cstr(&buf));

	/* add letter again */
	git_buf_putc(&buf, '+');
	cl_assert(git_buf_oom(&buf) == 0);
	cl_assert_equal_s("++", git_buf_cstr(&buf));

	/* let's try that a few times */
	for (i = 0; i < 16; ++i) {
		git_buf_putc(&buf, '+');
		cl_assert(git_buf_oom(&buf) == 0);
	}
	cl_assert_equal_s("++++++++++++++++++", git_buf_cstr(&buf));

	git_buf_free(&buf);

	/* add data */
	git_buf_put(&buf, "xo", 2);
	cl_assert(git_buf_oom(&buf) == 0);
	cl_assert_equal_s("xo", git_buf_cstr(&buf));

	/* add letter again */
	git_buf_put(&buf, "xo", 2);
	cl_assert(git_buf_oom(&buf) == 0);
	cl_assert_equal_s("xoxo", git_buf_cstr(&buf));

	/* let's try that a few times */
	for (i = 0; i < 16; ++i) {
		git_buf_put(&buf, "xo", 2);
		cl_assert(git_buf_oom(&buf) == 0);
	}
	cl_assert_equal_s("xoxoxoxoxoxoxoxoxoxoxoxoxoxoxoxoxoxo",
					   git_buf_cstr(&buf));

	git_buf_free(&buf);

	/* set to string */
	git_buf_sets(&buf, test_string);
	cl_assert(git_buf_oom(&buf) == 0);
	cl_assert_equal_s(test_string, git_buf_cstr(&buf));

	/* append string */
	git_buf_puts(&buf, test_string);
	cl_assert(git_buf_oom(&buf) == 0);
	cl_assert_equal_s(test_string_x2, git_buf_cstr(&buf));

	/* set to string again (should overwrite - not append) */
	git_buf_sets(&buf, test_string);
	cl_assert(git_buf_oom(&buf) == 0);
	cl_assert_equal_s(test_string, git_buf_cstr(&buf));

	/* test clear */
	git_buf_clear(&buf);
	cl_assert_equal_s("", git_buf_cstr(&buf));

	git_buf_free(&buf);

	/* test extracting data into buffer */
	git_buf_puts(&buf, REP4("0123456789"));
	cl_assert(git_buf_oom(&buf) == 0);

	git_buf_copy_cstr(data, sizeof(data), &buf);
	cl_assert_equal_s(REP4("0123456789"), data);
	git_buf_copy_cstr(data, 11, &buf);
	cl_assert_equal_s("0123456789", data);
	git_buf_copy_cstr(data, 3, &buf);
	cl_assert_equal_s("01", data);
	git_buf_copy_cstr(data, 1, &buf);
	cl_assert_equal_s("", data);

	git_buf_copy_cstr(data, sizeof(data), &buf);
	cl_assert_equal_s(REP4("0123456789"), data);

	git_buf_sets(&buf, REP256("x"));
	git_buf_copy_cstr(data, sizeof(data), &buf);
	/* since sizeof(data) == 128, only 127 bytes should be copied */
	cl_assert_equal_s(REP4(REP16("x")) REP16("x") REP16("x")
					   REP16("x") "xxxxxxxxxxxxxxx", data);

	git_buf_free(&buf);

	git_buf_copy_cstr(data, sizeof(data), &buf);
	cl_assert_equal_s("", data);
}

/* let's do some tests with larger buffers to push our limits */
void test_core_buffer__3(void)
{
	git_buf buf = GIT_BUF_INIT;

	/* set to string */
	git_buf_set(&buf, test_4096, 4096);
	cl_assert(git_buf_oom(&buf) == 0);
	cl_assert_equal_s(test_4096, git_buf_cstr(&buf));

	/* append string */
	git_buf_puts(&buf, test_4096);
	cl_assert(git_buf_oom(&buf) == 0);
	cl_assert_equal_s(test_8192, git_buf_cstr(&buf));

	/* set to string again (should overwrite - not append) */
	git_buf_set(&buf, test_4096, 4096);
	cl_assert(git_buf_oom(&buf) == 0);
	cl_assert_equal_s(test_4096, git_buf_cstr(&buf));

	git_buf_free(&buf);
}

/* let's try some producer/consumer tests */
void test_core_buffer__4(void)
{
	git_buf buf = GIT_BUF_INIT;
	int i;

	for (i = 0; i < 10; ++i) {
		git_buf_puts(&buf, "1234"); /* add 4 */
		cl_assert(git_buf_oom(&buf) == 0);
		git_buf_consume(&buf, buf.ptr + 2); /* eat the first two */
		cl_assert(strlen(git_buf_cstr(&buf)) == (size_t)((i + 1) * 2));
	}
	/* we have appended 1234 10x and removed the first 20 letters */
	cl_assert_equal_s("12341234123412341234", git_buf_cstr(&buf));

	git_buf_consume(&buf, NULL);
	cl_assert_equal_s("12341234123412341234", git_buf_cstr(&buf));

	git_buf_consume(&buf, "invalid pointer");
	cl_assert_equal_s("12341234123412341234", git_buf_cstr(&buf));

	git_buf_consume(&buf, buf.ptr);
	cl_assert_equal_s("12341234123412341234", git_buf_cstr(&buf));

	git_buf_consume(&buf, buf.ptr + 1);
	cl_assert_equal_s("2341234123412341234", git_buf_cstr(&buf));

	git_buf_consume(&buf, buf.ptr + buf.size);
	cl_assert_equal_s("", git_buf_cstr(&buf));

	git_buf_free(&buf);
}


static void
check_buf_append(
	const char* data_a,
	const char* data_b,
	const char* expected_data,
	size_t expected_size,
	size_t expected_asize)
{
	git_buf tgt = GIT_BUF_INIT;

	git_buf_sets(&tgt, data_a);
	cl_assert(git_buf_oom(&tgt) == 0);
	git_buf_puts(&tgt, data_b);
	cl_assert(git_buf_oom(&tgt) == 0);
	cl_assert_equal_s(expected_data, git_buf_cstr(&tgt));
	cl_assert(tgt.size == expected_size);
	if (expected_asize > 0)
		cl_assert(tgt.asize == expected_asize);

	git_buf_free(&tgt);
}

static void
check_buf_append_abc(
	const char* buf_a,
	const char* buf_b,
	const char* buf_c,
	const char* expected_ab,
	const char* expected_abc,
	const char* expected_abca,
	const char* expected_abcab,
	const char* expected_abcabc)
{
	git_buf buf = GIT_BUF_INIT;

	git_buf_sets(&buf, buf_a);
	cl_assert(git_buf_oom(&buf) == 0);
	cl_assert_equal_s(buf_a, git_buf_cstr(&buf));

	git_buf_puts(&buf, buf_b);
	cl_assert(git_buf_oom(&buf) == 0);
	cl_assert_equal_s(expected_ab, git_buf_cstr(&buf));

	git_buf_puts(&buf, buf_c);
	cl_assert(git_buf_oom(&buf) == 0);
	cl_assert_equal_s(expected_abc, git_buf_cstr(&buf));

	git_buf_puts(&buf, buf_a);
	cl_assert(git_buf_oom(&buf) == 0);
	cl_assert_equal_s(expected_abca, git_buf_cstr(&buf));

	git_buf_puts(&buf, buf_b);
	cl_assert(git_buf_oom(&buf) == 0);
	cl_assert_equal_s(expected_abcab, git_buf_cstr(&buf));

	git_buf_puts(&buf, buf_c);
	cl_assert(git_buf_oom(&buf) == 0);
	cl_assert_equal_s(expected_abcabc, git_buf_cstr(&buf));

	git_buf_free(&buf);
}

/* more variations on append tests */
void test_core_buffer__5(void)
{
	check_buf_append("", "", "", 0, 8);
	check_buf_append("a", "", "a", 1, 8);
	check_buf_append("", "a", "a", 1, 8);
	check_buf_append("", "a", "a", 1, 8);
	check_buf_append("a", "", "a", 1, 8);
	check_buf_append("a", "b", "ab", 2, 8);
	check_buf_append("", "abcdefgh", "abcdefgh", 8, 16);
	check_buf_append("abcdefgh", "", "abcdefgh", 8, 16);

	/* buffer with starting asize will grow to:
	 *  1 ->  2,  2 ->  3,  3 ->  5,  4 ->  6,  5 ->  8,  6 ->  9,
	 *  7 -> 11,  8 -> 12,  9 -> 14, 10 -> 15, 11 -> 17, 12 -> 18,
	 * 13 -> 20, 14 -> 21, 15 -> 23, 16 -> 24, 17 -> 26, 18 -> 27,
	 * 19 -> 29, 20 -> 30, 21 -> 32, 22 -> 33, 23 -> 35, 24 -> 36,
	 * ...
	 * follow sequence until value > target size,
	 * then round up to nearest multiple of 8.
	 */

	check_buf_append("abcdefgh", "/", "abcdefgh/", 9, 16);
	check_buf_append("abcdefgh", "ijklmno", "abcdefghijklmno", 15, 16);
	check_buf_append("abcdefgh", "ijklmnop", "abcdefghijklmnop", 16, 24);
	check_buf_append("0123456789", "0123456789",
					 "01234567890123456789", 20, 24);
	check_buf_append(REP16("x"), REP16("o"),
					 REP16("x") REP16("o"), 32, 40);

	check_buf_append(test_4096, "", test_4096, 4096, 4104);
	check_buf_append(test_4096, test_4096, test_8192, 8192, 9240);

	/* check sequences of appends */
	check_buf_append_abc("a", "b", "c",
						 "ab", "abc", "abca", "abcab", "abcabc");
	check_buf_append_abc("a1", "b2", "c3",
						 "a1b2", "a1b2c3", "a1b2c3a1",
						 "a1b2c3a1b2", "a1b2c3a1b2c3");
	check_buf_append_abc("a1/", "b2/", "c3/",
						 "a1/b2/", "a1/b2/c3/", "a1/b2/c3/a1/",
						 "a1/b2/c3/a1/b2/", "a1/b2/c3/a1/b2/c3/");
}

/* test swap */
void test_core_buffer__6(void)
{
	git_buf a = GIT_BUF_INIT;
	git_buf b = GIT_BUF_INIT;

	git_buf_sets(&a, "foo");
	cl_assert(git_buf_oom(&a) == 0);
	git_buf_sets(&b, "bar");
	cl_assert(git_buf_oom(&b) == 0);

	cl_assert_equal_s("foo", git_buf_cstr(&a));
	cl_assert_equal_s("bar", git_buf_cstr(&b));

	git_buf_swap(&a, &b);

	cl_assert_equal_s("bar", git_buf_cstr(&a));
	cl_assert_equal_s("foo", git_buf_cstr(&b));

	git_buf_free(&a);
	git_buf_free(&b);
}


/* test detach/attach data */
void test_core_buffer__7(void)
{
	const char *fun = "This is fun";
	git_buf a = GIT_BUF_INIT;
	char *b = NULL;

	git_buf_sets(&a, "foo");
	cl_assert(git_buf_oom(&a) == 0);
	cl_assert_equal_s("foo", git_buf_cstr(&a));

	b = git_buf_detach(&a);

	cl_assert_equal_s("foo", b);
	cl_assert_equal_s("", a.ptr);
	git__free(b);

	b = git_buf_detach(&a);

	cl_assert_equal_s(NULL, b);
	cl_assert_equal_s("", a.ptr);

	git_buf_free(&a);

	b = git__strdup(fun);
	git_buf_attach(&a, b, 0);

	cl_assert_equal_s(fun, a.ptr);
	cl_assert(a.size == strlen(fun));
	cl_assert(a.asize == strlen(fun) + 1);

	git_buf_free(&a);

	b = git__strdup(fun);
	git_buf_attach(&a, b, strlen(fun) + 1);

	cl_assert_equal_s(fun, a.ptr);
	cl_assert(a.size == strlen(fun));
	cl_assert(a.asize == strlen(fun) + 1);

	git_buf_free(&a);
}


static void
check_joinbuf_2(
	const char *a,
	const char *b,
	const char *expected)
{
	char sep = '/';
	git_buf buf = GIT_BUF_INIT;

	git_buf_join(&buf, sep, a, b);
	cl_assert(git_buf_oom(&buf) == 0);
	cl_assert_equal_s(expected, git_buf_cstr(&buf));
	git_buf_free(&buf);
}

static void
check_joinbuf_n_2(
	const char *a,
	const char *b,
	const char *expected)
{
	char sep = '/';
	git_buf buf = GIT_BUF_INIT;

	git_buf_sets(&buf, a);
	cl_assert(git_buf_oom(&buf) == 0);

	git_buf_join_n(&buf, sep, 1, b);
	cl_assert(git_buf_oom(&buf) == 0);
	cl_assert_equal_s(expected, git_buf_cstr(&buf));

	git_buf_free(&buf);
}

static void
check_joinbuf_n_4(
	const char *a,
	const char *b,
	const char *c,
	const char *d,
	const char *expected)
{
	char sep = ';';
	git_buf buf = GIT_BUF_INIT;
	git_buf_join_n(&buf, sep, 4, a, b, c, d);
	cl_assert(git_buf_oom(&buf) == 0);
	cl_assert_equal_s(expected, git_buf_cstr(&buf));
	git_buf_free(&buf);
}

/* test join */
void test_core_buffer__8(void)
{
	git_buf a = GIT_BUF_INIT;

	git_buf_join_n(&a, '/', 1, "foo");
	cl_assert(git_buf_oom(&a) == 0);
	cl_assert_equal_s("foo", git_buf_cstr(&a));

	git_buf_join_n(&a, '/', 1, "bar");
	cl_assert(git_buf_oom(&a) == 0);
	cl_assert_equal_s("foo/bar", git_buf_cstr(&a));

	git_buf_join_n(&a, '/', 1, "baz");
	cl_assert(git_buf_oom(&a) == 0);
	cl_assert_equal_s("foo/bar/baz", git_buf_cstr(&a));

	git_buf_free(&a);

	check_joinbuf_2("", "", "");
	check_joinbuf_2("", "a", "a");
	check_joinbuf_2("", "/a", "/a");
	check_joinbuf_2("a", "", "a/");
	check_joinbuf_2("a", "/", "a/");
	check_joinbuf_2("a", "b", "a/b");
	check_joinbuf_2("/", "a", "/a");
	check_joinbuf_2("/", "", "/");
	check_joinbuf_2("/a", "/b", "/a/b");
	check_joinbuf_2("/a", "/b/", "/a/b/");
	check_joinbuf_2("/a/", "b/", "/a/b/");
	check_joinbuf_2("/a/", "/b/", "/a/b/");
	check_joinbuf_2("/a/", "//b/", "/a/b/");
	check_joinbuf_2("/abcd", "/defg", "/abcd/defg");
	check_joinbuf_2("/abcd", "/defg/", "/abcd/defg/");
	check_joinbuf_2("/abcd/", "defg/", "/abcd/defg/");
	check_joinbuf_2("/abcd/", "/defg/", "/abcd/defg/");

	check_joinbuf_n_2("", "", "");
	check_joinbuf_n_2("", "a", "a");
	check_joinbuf_n_2("", "/a", "/a");
	check_joinbuf_n_2("a", "", "a/");
	check_joinbuf_n_2("a", "/", "a/");
	check_joinbuf_n_2("a", "b", "a/b");
	check_joinbuf_n_2("/", "a", "/a");
	check_joinbuf_n_2("/", "", "/");
	check_joinbuf_n_2("/a", "/b", "/a/b");
	check_joinbuf_n_2("/a", "/b/", "/a/b/");
	check_joinbuf_n_2("/a/", "b/", "/a/b/");
	check_joinbuf_n_2("/a/", "/b/", "/a/b/");
	check_joinbuf_n_2("/abcd", "/defg", "/abcd/defg");
	check_joinbuf_n_2("/abcd", "/defg/", "/abcd/defg/");
	check_joinbuf_n_2("/abcd/", "defg/", "/abcd/defg/");
	check_joinbuf_n_2("/abcd/", "/defg/", "/abcd/defg/");

	check_joinbuf_n_4("", "", "", "", "");
	check_joinbuf_n_4("", "a", "", "", "a;");
	check_joinbuf_n_4("a", "", "", "", "a;");
	check_joinbuf_n_4("", "", "", "a", "a");
	check_joinbuf_n_4("a", "b", "", ";c;d;", "a;b;c;d;");
	check_joinbuf_n_4("a", "b", "", ";c;d", "a;b;c;d");
	check_joinbuf_n_4("abcd", "efgh", "ijkl", "mnop", "abcd;efgh;ijkl;mnop");
	check_joinbuf_n_4("abcd;", "efgh;", "ijkl;", "mnop;", "abcd;efgh;ijkl;mnop;");
	check_joinbuf_n_4(";abcd;", ";efgh;", ";ijkl;", ";mnop;", ";abcd;efgh;ijkl;mnop;");
}

void test_core_buffer__9(void)
{
	git_buf buf = GIT_BUF_INIT;

	/* just some exhaustive tests of various separator placement */
	char *a[] = { "", "-", "a-", "-a", "-a-" };
	char *b[] = { "", "-", "b-", "-b", "-b-" };
	char sep[] = { 0, '-', '/' };
	char *expect_null[] = { "",    "-",     "a-",     "-a",     "-a-",
							"-",   "--",    "a--",    "-a-",    "-a--",
							"b-",  "-b-",   "a-b-",   "-ab-",   "-a-b-",
							"-b",  "--b",   "a--b",   "-a-b",   "-a--b",
							"-b-", "--b-",  "a--b-",  "-a-b-",  "-a--b-" };
	char *expect_dash[] = { "",    "-",     "a-",     "-a-",    "-a-",
							"-",   "-",     "a-",     "-a-",    "-a-",
							"b-",  "-b-",   "a-b-",   "-a-b-",  "-a-b-",
							"-b",  "-b",    "a-b",    "-a-b",   "-a-b",
							"-b-", "-b-",   "a-b-",   "-a-b-",  "-a-b-" };
	char *expect_slas[] = { "",    "-/",    "a-/",    "-a/",    "-a-/",
							"-",   "-/-",   "a-/-",   "-a/-",   "-a-/-",
							"b-",  "-/b-",  "a-/b-",  "-a/b-",  "-a-/b-",
							"-b",  "-/-b",  "a-/-b",  "-a/-b",  "-a-/-b",
							"-b-", "-/-b-", "a-/-b-", "-a/-b-", "-a-/-b-" };
	char **expect_values[] = { expect_null, expect_dash, expect_slas };
	char separator, **expect;
	unsigned int s, i, j;

	for (s = 0; s < sizeof(sep) / sizeof(char); ++s) {
		separator = sep[s];
		expect = expect_values[s];

		for (j = 0; j < sizeof(b) / sizeof(char*); ++j) {
			for (i = 0; i < sizeof(a) / sizeof(char*); ++i) {
				git_buf_join(&buf, separator, a[i], b[j]);
				cl_assert_equal_s(*expect, buf.ptr);
				expect++;
			}
		}
	}

	git_buf_free(&buf);
}

void test_core_buffer__10(void)
{
	git_buf a = GIT_BUF_INIT;

	cl_git_pass(git_buf_join_n(&a, '/', 1, "test"));
	cl_assert_equal_s(a.ptr, "test");
	cl_git_pass(git_buf_join_n(&a, '/', 1, "string"));
	cl_assert_equal_s(a.ptr, "test/string");
	git_buf_clear(&a);
	cl_git_pass(git_buf_join_n(&a, '/', 3, "test", "string", "join"));
	cl_assert_equal_s(a.ptr, "test/string/join");
	cl_git_pass(git_buf_join_n(&a, '/', 2, a.ptr, "more"));
	cl_assert_equal_s(a.ptr, "test/string/join/test/string/join/more");

	git_buf_free(&a);
}

void test_core_buffer__11(void)
{
	git_buf a = GIT_BUF_INIT;
	git_strarray t;
	char *t1[] = { "nothing", "in", "common" };
	char *t2[] = { "something", "something else", "some other" };
	char *t3[] = { "something", "some fun", "no fun" };
	char *t4[] = { "happy", "happier", "happiest" };
	char *t5[] = { "happiest", "happier", "happy" };
	char *t6[] = { "no", "nope", "" };
	char *t7[] = { "", "doesn't matter" };

	t.strings = t1;
	t.count = 3;
	cl_git_pass(git_buf_common_prefix(&a, &t));
	cl_assert_equal_s(a.ptr, "");

	t.strings = t2;
	t.count = 3;
	cl_git_pass(git_buf_common_prefix(&a, &t));
	cl_assert_equal_s(a.ptr, "some");

	t.strings = t3;
	t.count = 3;
	cl_git_pass(git_buf_common_prefix(&a, &t));
	cl_assert_equal_s(a.ptr, "");

	t.strings = t4;
	t.count = 3;
	cl_git_pass(git_buf_common_prefix(&a, &t));
	cl_assert_equal_s(a.ptr, "happ");

	t.strings = t5;
	t.count = 3;
	cl_git_pass(git_buf_common_prefix(&a, &t));
	cl_assert_equal_s(a.ptr, "happ");

	t.strings = t6;
	t.count = 3;
	cl_git_pass(git_buf_common_prefix(&a, &t));
	cl_assert_equal_s(a.ptr, "");

	t.strings = t7;
	t.count = 3;
	cl_git_pass(git_buf_common_prefix(&a, &t));
	cl_assert_equal_s(a.ptr, "");

	git_buf_free(&a);
}
