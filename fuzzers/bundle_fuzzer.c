/*
 * libgit2 bundle header fuzzer target.
 *
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include <stdio.h>

#include "git2.h"

#include "common.h"
#include "bundle.h"

#include "fuzzer_utils.h"
#include "standalone_driver.h"

#define SHORT_READ_MAX 7

#define SHA1_A "a65fedf39aefe402d3bb6e24df4d4f5fe4547750"
#define SHA1_B "a4a7dce85cf63874e984719f4fdd239f5145052f"

/*
 * A valid header that both parsers hold before and after each input, so
 * that every input replaces a populated result and is followed by reuse.
 * Its entries are out of sorted order so that reordering is detected.
 */
static const char fixed_header[] =
	"# v2 git bundle\n"
	"-" SHA1_B " a comment\n"
	"-" SHA1_A "\n"
	SHA1_A " refs/heads/main\n"
	SHA1_B " refs/heads/develop\n"
	"\n";

static const struct {
	const char *name;
	const char *oid;
} fixed_refs[] = {
	{ "refs/heads/main", SHA1_A },
	{ "refs/heads/develop", SHA1_B }
};

static const char *fixed_prereqs[] = { SHA1_B, SHA1_A };

#define CHECK(expr) \
	do { \
		if (!(expr)) \
			check_failed(#expr, __LINE__); \
	} while (0)

static void check_failed(const char *expr, int line)
{
	fprintf(stderr, "bundle_fuzzer.c:%d: check failed: %s\n", line, expr);
	abort();
}

typedef struct {
	const unsigned char *data;
	size_t len;
	size_t pos;
} short_reader;

/* Return at most a few bytes per call to exercise partial-line buffering. */
static int short_read(void *payload, void *buf, size_t len, size_t *out_read)
{
	short_reader *reader = payload;
	size_t remaining = reader->len - reader->pos;

	if (len > SHORT_READ_MAX)
		len = SHORT_READ_MAX;

	if (len > remaining)
		len = remaining;

	memcpy(buf, reader->data + reader->pos, len);
	reader->pos += len;

	*out_read = len;
	return 0;
}

static int parse_memory(
	git_bundle_parser *parser, const unsigned char *data, size_t size)
{
	return git_bundle_parser_parse_memory(parser, data, size);
}

static int parse_short(
	git_bundle_parser *parser, const unsigned char *data, size_t size)
{
	short_reader reader;

	reader.data = data;
	reader.len = size;
	reader.pos = 0;

	parser->read = short_read;
	parser->seek = NULL;
	parser->payload = &reader;

	return git_bundle_parser_parse(parser);
}

static void check_source_released(git_bundle_parser *parser)
{
	CHECK(parser->read == NULL);
	CHECK(parser->seek == NULL);
	CHECK(parser->payload == NULL);
	CHECK(parser->fd == 0);
	CHECK(parser->data == NULL);
	CHECK(parser->data_len == 0);
	CHECK(parser->data_pos == 0);
	CHECK(parser->offset == 0);
	CHECK(parser->eof == 0);
	CHECK(parser->buf.size == 0);
	CHECK(parser->buf.asize == 0);
}

static void check_header_cleared(git_bundle_header *header)
{
	CHECK(header->version == 0);
	CHECK(header->oid_type == 0);
	CHECK(header->pack_offset == 0);
	CHECK(header->prerequisites.size == 0);
	CHECK(header->prerequisites.ptr == NULL);
	CHECK(header->refs.length == 0);
	CHECK(header->refs.contents == NULL);
}

/* A fully parsed header: the pack follows a blank separator line. */
static void check_header_complete(
	git_bundle_header *header, const unsigned char *data, size_t size)
{
	git_remote_head *head;
	size_t i;

	CHECK(header->version == 2 || header->version == 3);
	CHECK(header->oid_type == GIT_OID_SHA1 ||
	      header->oid_type == GIT_OID_SHA256);

	/* the shortest header is the signature line and the separator */
	CHECK(header->pack_offset >= 17);
	CHECK(header->pack_offset <= size);
	CHECK(data[header->pack_offset - 1] == '\n');
	CHECK(data[header->pack_offset - 2] == '\n');

	for (i = 0; i < header->prerequisites.size; i++)
		CHECK(header->prerequisites.ptr[i].type == header->oid_type);

	for (i = 0; i < header->refs.length; i++) {
		head = git_vector_get(&header->refs, i);

		CHECK(head != NULL);
		CHECK(head->name != NULL);
		CHECK(strlen(head->name) > 0);
		CHECK(head->oid.type == header->oid_type);
	}
}

static void check_result(
	git_bundle_parser *parser,
	int error,
	const unsigned char *data,
	size_t size)
{
	git_bundle_header *header = &parser->header;

	check_source_released(parser);

	if (error == 0) {
		check_header_complete(header, data, size);
	} else if (error == GIT_ENOTSUPPORTED && header->pack_offset == 0) {
		/*
		 * An unknown object format is reported as soon as it is
		 * seen; capabilities precede everything but the signature.
		 */
		CHECK(header->version == 3);
		CHECK(header->oid_type == GIT_OID_SHA1);
		CHECK(header->prerequisites.size == 0);
		CHECK(header->refs.length == 0);
	} else if (error == GIT_ENOTSUPPORTED) {
		check_header_complete(header, data, size);
	} else {
		check_header_cleared(header);
	}
}

static void check_headers_equal(git_bundle_header *a, git_bundle_header *b)
{
	git_remote_head *ha, *hb;
	size_t i;

	CHECK(a->version == b->version);
	CHECK(a->oid_type == b->oid_type);
	CHECK(a->pack_offset == b->pack_offset);
	CHECK(a->prerequisites.size == b->prerequisites.size);
	CHECK(a->refs.length == b->refs.length);

	for (i = 0; i < a->prerequisites.size; i++)
		CHECK(git_oid_equal(&a->prerequisites.ptr[i],
			&b->prerequisites.ptr[i]));

	for (i = 0; i < a->refs.length; i++) {
		ha = git_vector_get(&a->refs, i);
		hb = git_vector_get(&b->refs, i);

		CHECK(strcmp(ha->name, hb->name) == 0);
		CHECK(git_oid_equal(&ha->oid, &hb->oid));
	}
}

static void check_oid(const git_oid *actual, const char *expected_str)
{
	git_oid expected;

	if (git_oid_from_string(&expected, expected_str, GIT_OID_SHA1) < 0)
		fuzzer_git_abort("git_oid_from_string");

	CHECK(git_oid_equal(&expected, actual));
}

static void check_fixed_header(git_bundle_parser *parser, int error)
{
	git_bundle_header *header = &parser->header;
	git_remote_head *head;
	size_t i;

	if (error < 0)
		fuzzer_git_abort("git_bundle_parser_parse");

	check_source_released(parser);

	CHECK(header->version == 2);
	CHECK(header->oid_type == GIT_OID_SHA1);
	CHECK(header->pack_offset == sizeof(fixed_header) - 1);

	CHECK(header->prerequisites.size == ARRAY_SIZE(fixed_prereqs));
	for (i = 0; i < ARRAY_SIZE(fixed_prereqs); i++)
		check_oid(&header->prerequisites.ptr[i], fixed_prereqs[i]);

	CHECK(header->refs.length == ARRAY_SIZE(fixed_refs));
	for (i = 0; i < ARRAY_SIZE(fixed_refs); i++) {
		head = git_vector_get(&header->refs, i);

		CHECK(strcmp(head->name, fixed_refs[i].name) == 0);
		check_oid(&head->oid, fixed_refs[i].oid);
	}
}

static void parse_fixed(git_bundle_parser *mem, git_bundle_parser *shrt)
{
	const unsigned char *data = (const unsigned char *)fixed_header;
	size_t size = sizeof(fixed_header) - 1;

	check_fixed_header(mem, parse_memory(mem, data, size));
	check_fixed_header(shrt, parse_short(shrt, data, size));
}

static void dispose_and_check(git_bundle_parser *parser)
{
	git_bundle_parser_dispose(parser);
	check_source_released(parser);
	check_header_cleared(&parser->header);
}

int LLVMFuzzerInitialize(int *argc, char ***argv)
{
	GIT_UNUSED(argc);
	GIT_UNUSED(argv);

	if (git_libgit2_init() < 0) {
		fprintf(stderr, "Failed to initialize libgit2\n");
		abort();
	}
	return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	git_bundle_parser mem = GIT_BUNDLE_PARSER_INIT;
	git_bundle_parser shrt = GIT_BUNDLE_PARSER_INIT;
	int mem_error, short_error;

	/* replace a populated result with the input */
	parse_fixed(&mem, &shrt);

	mem_error = parse_memory(&mem, data, size);
	check_result(&mem, mem_error, data, size);

	short_error = parse_short(&shrt, data, size);
	check_result(&shrt, short_error, data, size);

	CHECK(mem_error == short_error);
	check_headers_equal(&mem.header, &shrt.header);

	/* the parsers are reusable whatever the input did to them */
	parse_fixed(&mem, &shrt);

	dispose_and_check(&mem);
	dispose_and_check(&mem);
	dispose_and_check(&shrt);
	dispose_and_check(&shrt);

	/* expected parse failures leave an error set; don't leak it */
	git_error_clear();
	return 0;
}
