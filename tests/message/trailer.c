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
	assert_trailers(
		"Message\n"
		"\n"
		"Signed-off-by: foo@bar.com\n"
		"Signed-off-by: someone@else.com\n"
	,
		(struct trailer[]){
			{ "Signed-off-by", "foo@bar.com" },
			{ "Signed-off-by", "someone@else.com" },
			{ NULL, NULL },
		}
	);
}

void test_message_trailer__no_whitespace(void)
{
	assert_trailers(
		"Message\n"
		"\n"
		"Key:value\n"
	,
		(struct trailer[]){
			{ "Key", "value" },
			{ NULL, NULL },
		}
	);
}

void test_message_trailer__extra_whitespace(void)
{
	assert_trailers(
		"Message\n"
		"\n"
		"Key   :   value\n"
	,
		(struct trailer[]){
			{ "Key", "value" },
			{ NULL, NULL },
		}
	);
}

void test_message_trailer__no_newline(void)
{
	assert_trailers(
		"Message\n"
		"\n"
		"Key: value"
	,
		(struct trailer[]){
			{ "Key", "value" },
			{ NULL, NULL },
		}
	);
}

void test_message_trailer__not_last_paragraph(void)
{
	assert_trailers(
		"Message\n"
		"\n"
		"Key: value\n"
		"\n"
		"More stuff\n"
	,
		(struct trailer[]){
			{ NULL, NULL },
		}
	);
}

void test_message_trailer__conflicts(void)
{
	assert_trailers(
		"Message\n"
		"\n"
		"Key: value\n"
		"\n"
		"Conflicts:\n"
		"\tfoo.c\n"
	,
		(struct trailer[]){
			{ "Key", "value" },
			{ NULL, NULL },
		}
	);
}

void test_message_trailer__patch(void)
{
	assert_trailers(
		"Message\n"
		"\n"
		"Key: value\n"
		"\n"
		"---\n"
		"More: stuff\n"
	,
		(struct trailer[]){
			{ "Key", "value" },
			{ NULL, NULL },
		}
	);
}

void test_message_trailer__continuation(void)
{
	assert_trailers(
		"Message\n"
		"\n"
		"A: b\n"
		" c\n"
		"D: e\n"
		" f: g h\n"
		"I: j\n"
	,
		(struct trailer[]){
			{ "A", "b\n c" },
			{ "D", "e\n f: g h" },
			{ "I", "j" },
			{ NULL, NULL },
		}
	);
}

void test_message_trailer__invalid(void)
{
	assert_trailers(
		"Message\n"
		"\n"
		"Signed-off-by: some@one.com\n"
		"Not a trailer\n"
		"Another: trailer\n"
	,
		(struct trailer[]){
			{ "Signed-off-by", "some@one.com" },
			{ "Another", "trailer" },
			{ NULL, NULL },
		}
	);
}
