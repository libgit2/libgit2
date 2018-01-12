#include "clar_libgit2.h"
#include "message.h"

struct trailer {
	const char *key;
	const char *value;
};

struct cb_state {
	struct trailer *trailer;
};

static int trailer_cb(const char *key, const char *value, void *st_)
{
	struct cb_state *st = st_;

	cl_assert_equal_s(st->trailer->key, key);
	cl_assert_equal_s(st->trailer->value, value);

	st->trailer++;

	return 0;
}

static void assert_trailers(const char *message, struct trailer *trailers)
{
	struct cb_state st = { trailers };

	int rc = git_message_trailers(message, trailer_cb, &st);

	cl_assert_equal_s(NULL, st.trailer->key);
	cl_assert_equal_s(NULL, st.trailer->value);

	cl_assert_equal_i(0, rc);
}

void test_message_trailer__simple(void)
{
	struct trailer trailers[] = {
		{"Signed-off-by", "foo@bar.com"},
		{"Signed-off-by", "someone@else.com"},
		{NULL, NULL},
	};

	assert_trailers(
		"Message\n"
		"\n"
		"Signed-off-by: foo@bar.com\n"
		"Signed-off-by: someone@else.com\n"
	, trailers);
}

void test_message_trailer__no_whitespace(void)
{
	struct trailer trailers[] = {
		{"Key", "value"},
		{NULL, NULL},
	};

	assert_trailers(
		"Message\n"
		"\n"
		"Key:value\n"
	, trailers);
}

void test_message_trailer__extra_whitespace(void)
{
	struct trailer trailers[] = {
		{"Key", "value"},
		{NULL, NULL},
	};

	assert_trailers(
		"Message\n"
		"\n"
		"Key   :   value\n"
	, trailers);
}

void test_message_trailer__no_newline(void)
{
	struct trailer trailers[] = {
		{"Key", "value"},
		{NULL, NULL},
	};

	assert_trailers(
		"Message\n"
		"\n"
		"Key: value"
	, trailers);
}

void test_message_trailer__not_last_paragraph(void)
{
	struct trailer trailers[] = {
		{NULL, NULL},
	};

	assert_trailers(
		"Message\n"
		"\n"
		"Key: value\n"
		"\n"
		"More stuff\n"
	, trailers);
}

void test_message_trailer__conflicts(void)
{
	struct trailer trailers[] = {
		{"Key", "value"},
		{NULL, NULL},
	};

	assert_trailers(
		"Message\n"
		"\n"
		"Key: value\n"
		"\n"
		"Conflicts:\n"
		"\tfoo.c\n"
	, trailers);
}

void test_message_trailer__patch(void)
{
	struct trailer trailers[] = {
		{"Key", "value"},
		{NULL, NULL},
	};

	assert_trailers(
		"Message\n"
		"\n"
		"Key: value\n"
		"\n"
		"---\n"
		"More: stuff\n"
	, trailers);
}

void test_message_trailer__continuation(void)
{
	struct trailer trailers[] = {
		{"A", "b\n c"},
		{"D", "e\n f: g h"},
		{"I", "j"},
		{NULL, NULL},
	};

	assert_trailers(
		"Message\n"
		"\n"
		"A: b\n"
		" c\n"
		"D: e\n"
		" f: g h\n"
		"I: j\n"
	, trailers);
}

void test_message_trailer__invalid(void)
{
	struct trailer trailers[] = {
		{"Signed-off-by", "some@one.com"},
		{"Another", "trailer"},
		{NULL, NULL},
	};

	assert_trailers(
		"Message\n"
		"\n"
		"Signed-off-by: some@one.com\n"
		"Not a trailer\n"
		"Another: trailer\n"
	, trailers);
}

void test_message_trailer__iterator_simple(void)
{
	git_message_trailer_iterator *iterator;
	const char *key;
	const char *value;
	int rc;

	rc = git_message_trailer_iterator_new(
		&iterator,
		"Message\n"
		"\n"
		"Signed-off-by: foo@bar.com\n"
		"Signed-off-by: someone@else.com\n");

	cl_assert(rc == 0);

	rc = git_message_trailer_iterator_next(&key, &value, iterator);

	cl_assert(rc == 0);

	cl_assert_equal_s("Signed-off-by", key);
	cl_assert_equal_s("foo@bar.com", value);

	rc = git_message_trailer_iterator_next(&key, &value, iterator);

	cl_assert(rc == 0);

	cl_assert_equal_s("Signed-off-by", key);
	cl_assert_equal_s("someone@else.com", value);

	rc = git_message_trailer_iterator_next(&key, &value, iterator);

	cl_assert(rc == GIT_ITEROVER);

	git_message_trailer_iterator_free(iterator);
}

void test_message_trailer__iterator_no_whitespace(void)
{
	git_message_trailer_iterator *iterator;
	const char *key;
	const char *value;
	int rc;

	rc = git_message_trailer_iterator_new(
		&iterator,
		"Message\n"
		"\n"
		"Key:value\n");

	cl_assert(rc == 0);

	rc = git_message_trailer_iterator_next(&key, &value, iterator);

	cl_assert(rc == 0);

	cl_assert_equal_s("Key", key);
	cl_assert_equal_s("value", value);

	rc = git_message_trailer_iterator_next(&key, &value, iterator);

	cl_assert(rc == GIT_ITEROVER);

	git_message_trailer_iterator_free(iterator);
}

void test_message_trailer__iterator_no_newline(void)
{
	git_message_trailer_iterator *iterator;
	const char *key;
	const char *value;
	int rc;

	rc = git_message_trailer_iterator_new(
		&iterator,
		"Message\n"
		"\n"
		"Key:value");

	cl_assert(rc == 0);

	rc = git_message_trailer_iterator_next(&key, &value, iterator);

	cl_assert(rc == 0);

	cl_assert_equal_s("Key", key);
	cl_assert_equal_s("value", value);

	rc = git_message_trailer_iterator_next(&key, &value, iterator);

	cl_assert(rc == GIT_ITEROVER);

	git_message_trailer_iterator_free(iterator);
}

void test_message_trailer__iterator_not_last_paragraph(void)
{
	git_message_trailer_iterator *iterator;
	const char *key;
	const char *value;
	int rc;

	rc = git_message_trailer_iterator_new(
		&iterator,
		"Message\n"
		"\n"
		"Key: value\n"
		"\n"
		"More stuff\n");

	cl_assert(rc == 0);

	rc = git_message_trailer_iterator_next(&key, &value, iterator);

	cl_assert(rc == GIT_ITEROVER);

	git_message_trailer_iterator_free(iterator);
}

void test_message_trailer__iterator_conflicts(void)
{
	git_message_trailer_iterator *iterator;
	const char *key;
	const char *value;
	int rc;

	rc = git_message_trailer_iterator_new(
		&iterator,
		"Message\n"
		"\n"
		"Key: value\n"
		"\n"
		"Conflicts:\n"
		"\tfoo.c\n");

	cl_assert(rc == 0);

	rc = git_message_trailer_iterator_next(&key, &value, iterator);

	cl_assert(rc == 0);

	cl_assert_equal_s("Key", key);
	cl_assert_equal_s("value", value);

	rc = git_message_trailer_iterator_next(&key, &value, iterator);

	cl_assert(rc == GIT_ITEROVER);

	git_message_trailer_iterator_free(iterator);
}

void test_message_trailer__iterator_patch(void)
{
	git_message_trailer_iterator *iterator;
	const char *key;
	const char *value;
	int rc;

	rc = git_message_trailer_iterator_new(
		&iterator,
		"Message\n"
		"\n"
		"Key: value\n"
		"\n"
		"---\n"
		"More: stuff\n");

	cl_assert(rc == 0);

	rc = git_message_trailer_iterator_next(&key, &value, iterator);

	cl_assert(rc == 0);

	cl_assert_equal_s("Key", key);
	cl_assert_equal_s("value", value);

	rc = git_message_trailer_iterator_next(&key, &value, iterator);

	cl_assert(rc == GIT_ITEROVER);

	git_message_trailer_iterator_free(iterator);
}

void test_message_trailer__iterator_continuation(void)
{
	git_message_trailer_iterator *iterator;
	const char *key;
	const char *value;
	int rc;

	rc = git_message_trailer_iterator_new(
		&iterator,
		"Message\n"
		"\n"
		"A: b\n"
		" c\n"
		"D: e\n"
		" f: g h\n"
		"I: j\n");

	cl_assert(rc == 0);

	rc = git_message_trailer_iterator_next(&key, &value, iterator);

	cl_assert(rc == 0);

	cl_assert_equal_s("A", key);
	cl_assert_equal_s("b\n c", value);

	rc = git_message_trailer_iterator_next(&key, &value, iterator);

	cl_assert(rc == 0);

	cl_assert_equal_s("D", key);
	cl_assert_equal_s("e\n f: g h", value);

	rc = git_message_trailer_iterator_next(&key, &value, iterator);

	cl_assert(rc == 0);

	cl_assert_equal_s("I", key);
	cl_assert_equal_s("j", value);

	rc = git_message_trailer_iterator_next(&key, &value, iterator);

	cl_assert(rc == GIT_ITEROVER);

	git_message_trailer_iterator_free(iterator);
}

void test_message_trailer__iterator_invalid(void)
{
	git_message_trailer_iterator *iterator;
	const char *key;
	const char *value;
	int rc;

	rc = git_message_trailer_iterator_new(
		&iterator,
		"Message\n"
		"\n"
		"Signed-off-by: some@one.com\n"
		"Not a trailer\n"
		"Another: trailer\n");

	cl_assert(rc == 0);

	rc = git_message_trailer_iterator_next(&key, &value, iterator);

	cl_assert(rc == 0);

	cl_assert_equal_s("Signed-off-by", key);
	cl_assert_equal_s("some@one.com", value);

	rc = git_message_trailer_iterator_next(&key, &value, iterator);

	cl_assert(rc == 0);

	cl_assert_equal_s("Another", key);
	cl_assert_equal_s("trailer", value);

	rc = git_message_trailer_iterator_next(&key, &value, iterator);

	cl_assert(rc == GIT_ITEROVER);

	git_message_trailer_iterator_free(iterator);
}
