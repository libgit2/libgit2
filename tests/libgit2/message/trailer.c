#include "clar_libgit2.h"


static assert_trailer_array(
        const git_message_trailer_array *actual,
        git_message_trailer *expected)

{
	size_t i, count;
	for (i = 0, count = 0; expected[i].key != NULL; i++, count++)
		;

	cl_assert_equal_i((int)actual->count, (int)count); /* see issue #7095. */

	for (i = 0; i < actual->count; i++) {
		cl_assert_equal_s(actual->trailers[i].key, expected[i].key);
		cl_assert_equal_s(
		        actual->trailers[i].value, expected[i].value);
	}
}

static void assert_trailers(const char *message, git_message_trailer *trailers)
{
	git_message_trailer_array arr;

	int rc = git_message_trailers(&arr, message);

	cl_assert_equal_i(0, rc);

	assert_trailer_array(&arr, trailers);

	git_message_trailer_array_free(&arr);
}

static void assert_trailers_ext(
        const char *message,
        git_message_trailer *trailers,
        const git_message_trailers_options *opts)
{
	git_message_trailer_array arr;

	int rc = git_message_trailers_ext(&arr, message, opts);

	cl_assert_equal_i(0, rc);

	assert_trailer_array(&arr, trailers);

	git_message_trailer_array_free(&arr);
}

void test_message_trailer__simple(void)
{
	git_message_trailer trailers[] = {
		{"Signed-off-by", "foo@bar.com"},
		{"Signed-off-by", "someone@else.com"},
		{NULL, NULL},
	};

	const char* message = "Message\n"
		"\n"
		"Signed-off-by: foo@bar.com\n"
		"Signed-off-by: someone@else.com\n";

	assert_trailers(message, trailers);
	assert_trailers_ext(message, trailers,NULL);

	git_message_trailers_options options =
	        GIT_MESSAGE_TRAILERS_OPTIONS_INIT;
	assert_trailers_ext(message, trailers,&options);

	/* The options all make no difference in this simple case. */
	options.trim_empty = TRUE;
	options.no_divider = TRUE;
	options.unfold = TRUE;
	assert_trailers_ext(message, trailers, &options);
}

void test_message_trailer__no_whitespace(void)
{
	git_message_trailer trailers[] = {
		{"Key", "value"},
		{NULL, NULL},
	};

	assert_trailers(
		"Message\n"
		"\n"
		"Key:value\n"
	, trailers);
}

void test_message_trailer__no_empty_line(void)
{
	git_message_trailer trailers[] = {
		{ NULL, NULL },
	};

	assert_trailers(
	        "Message\n"
	        "Key:value\n",
	        trailers);
}

void test_message_trailer__extra_whitespace(void)
{
	git_message_trailer trailers[] = {
		{"Key", "value with leading and trailing spaces"},
		{NULL, NULL},
	};

	assert_trailers(
		"Message\n"
		"\n"
		"Key   :   value with leading and trailing spaces  \n"
	, trailers);
}

void test_message_trailer__no_trailing_newline(void)
{
	git_message_trailer trailers[] = {
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
	git_message_trailer trailers[] = {
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

void test_message_trailer__empty_value(void)
{
	git_message_trailer trailers[] = {
		{ "EmptyValue", "" },
		{ "Another", "trailer here" },
		{ "YetAnother", "trailer" },
		{ NULL, NULL },
	};
	const char *message = "Message\n"
	                      "\n"
	                      "EmptyValue:     \n"
	                      "Another: trailer here\n"
	                      "YetAnother: trailer\n";
	assert_trailers(message, trailers);
	assert_trailers_ext(message, trailers,NULL);

    	git_message_trailers_options options =
	        GIT_MESSAGE_TRAILERS_OPTIONS_INIT;

	assert_trailers_ext(message, trailers, &options);

	options.trim_empty = TRUE;
	assert_trailers_ext(message, &trailers[1], &options);
}

void test_message_trailer__conflicts(void)
{
	git_message_trailer trailers[] = {
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
	/* When the "---" line is treated as a divider the
	 * "Key" line is a trailer.
	 */
	git_message_trailer trailers[] = {
		{"Key", "value"},
		{NULL, NULL},
	};

	/* When the "---" is not treated as a divider the
	 * "More" line is the trailer.
	 */
	git_message_trailer ext_trailers[] = {
		{ "More", "stuff" },
		{ NULL, NULL },
	};
	const char *message = "Message\n"
	                      "\n"
	                      "Key: value\n"
	                      "\n"
	                      "---\n"
	                      "\n"
	                      "More: stuff\n";
	assert_trailers(message, trailers);

	git_message_trailers_options options =
	        GIT_MESSAGE_TRAILERS_OPTIONS_INIT;
	assert_trailers_ext(message, trailers, &options);

	options.no_divider = TRUE;
	assert_trailers_ext(message, ext_trailers, &options);
}

void test_message_trailer__groups(void)
{
	git_message_trailer trailers[] = {
		{ "More", "stuff" },
		{ NULL, NULL },
	};

	const char *message = "Message\n"
	                      "\n"
	                      "Key: value\n"
	                      "\n"
	                      "A non-trailer line between two lines that look like trailers\n"
	                      "\n"
	                      "More: stuff\n";
	assert_trailers(message, trailers);

	git_message_trailers_options options =
	        GIT_MESSAGE_TRAILERS_OPTIONS_INIT;
	assert_trailers_ext(message, trailers, &options);

	options.no_divider = TRUE;
	assert_trailers_ext(message, trailers, &options);
}

void test_message_trailer__continuation(void)
{
	git_message_trailer trailers[] = {
		{"A", "bxy\n    cdef"},
		{"D", "e\n    f: g  h"},
		{"I", "j"},
		{NULL, NULL},
	};

	git_message_trailer unfolded_trailers[] = {
		{ "A", "bxy cdef" },
		{ "D", "e f: g  h" },
		{ "I", "j" },
		{ NULL, NULL },
	};

	const char *message = "Message\n"
	                "\n"
	                "A: bxy\n"
	                "    cdef\n"
	                "D: e\n"
	                "    f: g  h\n"
	                "I: j\n";
	assert_trailers(message, trailers);

	git_message_trailers_options options =
	        GIT_MESSAGE_TRAILERS_OPTIONS_INIT;
	assert_trailers_ext(message, trailers, &options);

	options.unfold = TRUE;
	assert_trailers_ext(message, unfolded_trailers, &options);
}

void test_message_trailer__continuation_tab(void)
{
	git_message_trailer trailers[] = {
		{ "A", "b\n c" },
		{ "D", "e\n\t\tf: g \th" },
		{ "I", "j" },
		{ NULL, NULL },
	};

	git_message_trailer unfolded_trailers[] = {
		{ "A", "b c" },
		{ "D", "e f: g \th" },
		{ "I", "j" },
		{ NULL, NULL },
	};

	const char *message = "Message\n"
	                      "\n"
	                      "A: b\n"
	                      " c\n"
	                      "D: e\n"
	                      "\t\tf: g \th\n"
	                      "I: j\n";
	assert_trailers(message, trailers);

	git_message_trailers_options options =
	        GIT_MESSAGE_TRAILERS_OPTIONS_INIT;
	assert_trailers_ext(message, trailers, &options);

	options.unfold = TRUE;
	assert_trailers_ext(message, unfolded_trailers, &options);
}

void test_message_trailer__invalid(void)
{
	/* A badly-formed trailer between two valid trailers
	 * is ignored as long as there are no empty lines inbetween.
	 */
	git_message_trailer trailers[] = {
		{"Signed-off-by", "some@one.com"},
		{"Another", "trailer"},
		{NULL, NULL},
	};

	const char *message = "Message\n"
	                      "\n"
	                      "Signed-off-by: some@one.com\n"
	                      "Not a trailer\n"
	                      "Another: trailer\n";
	assert_trailers(message, trailers);

	git_message_trailers_options options =
	        GIT_MESSAGE_TRAILERS_OPTIONS_INIT;
	options.unfold = TRUE;
	assert_trailers_ext(message, trailers, &options);
}

void test_message_trailer__ignores_dashes(void)
{
	/* More than three dashes is not a divider. */
	git_message_trailer trailers[] = {
		{ "Signed-off-by", "some@one.com" },
		{ "Another", "trailer" },
		{ NULL, NULL },
	};

	assert_trailers(
		"Message\n"
		"\n"
		"Markdown header\n"
		"---------------\n"
		"Lorem ipsum\n"
		"\n"
		"Signed-off-by: some@one.com\n"
		"Another: trailer\n"
	, trailers);
}
