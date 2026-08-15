#include "clar_libgit2.h"

#include "bundle.h"
#include "futils.h"
#include "posix.h"

#include "git2/sys/odb_backend.h"

#define TMP_BUNDLE "parse_tmp.bundle"

#define SHA1_A "a65fedf39aefe402d3bb6e24df4d4f5fe4547750"
#define SHA1_B "a4a7dce85cf63874e984719f4fdd239f5145052f"

#define SHA256_A "decaff3051968d1f3a2defd3d4a80ced03101555e1fd8913b3544026c0717d4f"
#define SHA256_B "a4813ef6708e6011e8187224297e83e4a285f58bf5eabb1db270351388603c95"

#define V2_HEADER \
	"# v2 git bundle\n" \
	"-" SHA1_A " a comment\n" \
	SHA1_B " refs/heads/br2\n" \
	SHA1_A " HEAD\n" \
	"\n"

#define PACK_BYTES "PACK\x00\x00\x00\x02rest"

void test_bundle_parse__cleanup(void)
{
	cl_fixture_cleanup(TMP_BUNDLE);
	cl_fixture_cleanup("prereq.git");
}

static int parse_memory(
	git_bundle_parser *parser, const char *data, size_t len)
{
	return git_bundle_parser_parse_memory(parser, data, len);
}

static int parse_file(
	git_bundle_parser *parser, const char *data, size_t len, git_off_t *out_pos)
{
	int fd, error;

	cl_must_pass(fd = p_open(TMP_BUNDLE, O_RDWR | O_CREAT | O_TRUNC, 0666));
	cl_must_pass(p_write(fd, data, len));
	cl_must_pass(p_lseek(fd, 0, SEEK_SET));

	error = git_bundle_parser_parse_fd(parser, fd);

	if (out_pos)
		cl_must_pass(*out_pos = p_lseek(fd, 0, SEEK_CUR));

	p_close(fd);

	return error;
}

/* the parser must not hold on to anything it was only lent */
static void assert_source_released(git_bundle_parser *parser)
{
	cl_assert(parser->read == NULL);
	cl_assert(parser->seek == NULL);
	cl_assert(parser->payload == NULL);
	cl_assert_equal_i(0, parser->fd);
	cl_assert(parser->data == NULL);
	cl_assert_equal_i(0, (int)parser->data_len);
	cl_assert_equal_i(0, (int)parser->data_pos);
	cl_assert_equal_i(0, (int)parser->offset);
	cl_assert_equal_i(0, (int)parser->eof);
	cl_assert_equal_i(0, (int)parser->buf.size);
	cl_assert_equal_i(0, (int)parser->buf.asize);
}

static void assert_headers_equal(
	git_bundle_header *a, git_bundle_header *b)
{
	size_t i;

	cl_assert_equal_i(a->version, b->version);
	cl_assert_equal_i(a->oid_type, b->oid_type);
	cl_assert_equal_i((int)a->pack_offset, (int)b->pack_offset);
	cl_assert_equal_i((int)a->prerequisites.size, (int)b->prerequisites.size);
	cl_assert_equal_i((int)a->refs.length, (int)b->refs.length);

	for (i = 0; i < a->prerequisites.size; i++)
		cl_assert(git_oid_equal(&a->prerequisites.ptr[i],
			&b->prerequisites.ptr[i]));

	for (i = 0; i < a->refs.length; i++) {
		git_remote_head *ha = git_vector_get(&a->refs, i);
		git_remote_head *hb = git_vector_get(&b->refs, i);

		cl_assert_equal_s(ha->name, hb->name);
		cl_assert(git_oid_equal(&ha->oid, &hb->oid));
	}
}

/*
 * Parse through both backings, assert they agree, and hand the
 * in-memory result back to the caller.
 */
static int parse_both(
	git_bundle_parser *parser, const char *data, size_t len)
{
	git_bundle_parser from_file = GIT_BUNDLE_PARSER_INIT;
	git_off_t pos = 0;
	int mem_error, file_error;

	mem_error = parse_memory(parser, data, len);
	file_error = parse_file(&from_file, data, len, &pos);

	cl_assert_equal_i(mem_error, file_error);

	assert_source_released(parser);
	assert_source_released(&from_file);

	if (mem_error == 0) {
		assert_headers_equal(&parser->header, &from_file.header);

		/* the descriptor is left at the first byte of the pack */
		cl_assert_equal_i((int)parser->header.pack_offset, (int)pos);
	}

	git_bundle_parser_dispose(&from_file);

	return mem_error;
}

static void assert_invalid(const char *data)
{
	git_bundle_parser parser = GIT_BUNDLE_PARSER_INIT;

	cl_assert_equal_i(GIT_EINVALID, parse_both(&parser, data, strlen(data)));
	cl_assert_equal_i(GIT_ERROR_INVALID, git_error_last()->klass);

	git_bundle_parser_dispose(&parser);
}

static void assert_unsupported(const char *data)
{
	git_bundle_parser parser = GIT_BUNDLE_PARSER_INIT;

	cl_assert_equal_i(GIT_ENOTSUPPORTED,
		parse_both(&parser, data, strlen(data)));
	cl_assert_equal_i(GIT_ERROR_INVALID, git_error_last()->klass);
	cl_assert_equal_i(3, parser.header.version);

	git_bundle_parser_dispose(&parser);
}

void test_bundle_parse__v2_sha1(void)
{
	git_bundle_parser parser = GIT_BUNDLE_PARSER_INIT;
	const char *data = V2_HEADER PACK_BYTES;
	git_remote_head *head;
	git_oid expected;

	cl_git_pass(parse_both(&parser, data, strlen(V2_HEADER) + 12));

	cl_assert_equal_i(2, parser.header.version);
	cl_assert_equal_i(GIT_OID_SHA1, parser.header.oid_type);
	cl_assert_equal_i((int)strlen(V2_HEADER), (int)parser.header.pack_offset);

	/* prerequisites are preserved; their comments are not */
	cl_assert_equal_i(1, (int)parser.header.prerequisites.size);
	cl_git_pass(git_oid_from_string(&expected, SHA1_A, GIT_OID_SHA1));
	cl_assert(git_oid_equal(&expected, &parser.header.prerequisites.ptr[0]));

	/* references keep their names, ids, and input order */
	cl_assert_equal_i(2, (int)parser.header.refs.length);

	head = git_vector_get(&parser.header.refs, 0);
	cl_assert_equal_s("refs/heads/br2", head->name);
	cl_git_pass(git_oid_from_string(&expected, SHA1_B, GIT_OID_SHA1));
	cl_assert(git_oid_equal(&expected, &head->oid));

	head = git_vector_get(&parser.header.refs, 1);
	cl_assert_equal_s("HEAD", head->name);
	cl_assert(head->symref_target == NULL);

	git_bundle_parser_dispose(&parser);
}

void test_bundle_parse__v3_sha1(void)
{
	git_bundle_parser parser = GIT_BUNDLE_PARSER_INIT;
	const char *data =
		"# v3 git bundle\n"
		"@object-format=sha1\n"
		SHA1_A " refs/heads/master\n"
		"\n";

	cl_git_pass(parse_both(&parser, data, strlen(data)));

	cl_assert_equal_i(3, parser.header.version);
	cl_assert_equal_i(GIT_OID_SHA1, parser.header.oid_type);
	cl_assert_equal_i(1, (int)parser.header.refs.length);

	git_bundle_parser_dispose(&parser);
}

void test_bundle_parse__v3_sha256(void)
{
	git_bundle_parser parser = GIT_BUNDLE_PARSER_INIT;
	git_remote_head *head;
	git_oid expected;
	const char *data =
		"# v3 git bundle\n"
		"@object-format=sha256\n"
		"-" SHA256_B " boundary\n"
		SHA256_A " refs/heads/master\n"
		"\n";

	cl_git_pass(parse_both(&parser, data, strlen(data)));

	cl_assert_equal_i(3, parser.header.version);
	cl_assert_equal_i(GIT_OID_SHA256, parser.header.oid_type);

	cl_assert_equal_i(1, (int)parser.header.prerequisites.size);
	cl_git_pass(git_oid_from_string(&expected, SHA256_B, GIT_OID_SHA256));
	cl_assert(git_oid_equal(&expected, &parser.header.prerequisites.ptr[0]));

	cl_assert_equal_i(1, (int)parser.header.refs.length);
	head = git_vector_get(&parser.header.refs, 0);
	cl_assert_equal_s("refs/heads/master", head->name);
	cl_git_pass(git_oid_from_string(&expected, SHA256_A, GIT_OID_SHA256));
	cl_assert(git_oid_equal(&expected, &head->oid));

	git_bundle_parser_dispose(&parser);
}

void test_bundle_parse__v2_default_is_sha1(void)
{
	git_bundle_parser parser = GIT_BUNDLE_PARSER_INIT;
	const char *data =
		"# v2 git bundle\n"
		SHA1_A " refs/heads/master\n"
		"\n";

	cl_git_pass(parse_both(&parser, data, strlen(data)));
	cl_assert_equal_i(GIT_OID_SHA1, parser.header.oid_type);

	git_bundle_parser_dispose(&parser);
}

void test_bundle_parse__v3_default_is_sha1(void)
{
	git_bundle_parser parser = GIT_BUNDLE_PARSER_INIT;
	const char *data =
		"# v3 git bundle\n"
		SHA1_A " refs/heads/master\n"
		"\n";

	cl_git_pass(parse_both(&parser, data, strlen(data)));
	cl_assert_equal_i(GIT_OID_SHA1, parser.header.oid_type);

	git_bundle_parser_dispose(&parser);
}

void test_bundle_parse__empty_header(void)
{
	git_bundle_parser parser = GIT_BUNDLE_PARSER_INIT;
	const char *data = "# v2 git bundle\n\n";

	cl_git_pass(parse_both(&parser, data, strlen(data)));

	cl_assert_equal_i(0, (int)parser.header.refs.length);
	cl_assert_equal_i(0, (int)parser.header.prerequisites.size);
	cl_assert_equal_i((int)strlen(data), (int)parser.header.pack_offset);

	git_bundle_parser_dispose(&parser);
}

void test_bundle_parse__long_and_chunk_spanning_lines(void)
{
	git_bundle_parser parser = GIT_BUNDLE_PARSER_INIT;
	git_str data = GIT_STR_INIT, name = GIT_STR_INIT;
	git_remote_head *head;
	size_t i;

	cl_git_pass(git_str_puts(&name, "refs/heads/"));

	for (i = 0; i < 20000; i++)
		cl_git_pass(git_str_putc(&name, 'a'));

	cl_git_pass(git_str_puts(&data, "# v2 git bundle\n"));
	cl_git_pass(git_str_printf(&data, "%s %s\n", SHA1_A, name.ptr));
	cl_git_pass(git_str_puts(&data, "\n"));

	cl_git_pass(parse_both(&parser, data.ptr, data.size));

	cl_assert_equal_i(1, (int)parser.header.refs.length);
	head = git_vector_get(&parser.header.refs, 0);
	cl_assert_equal_s(name.ptr, head->name);
	cl_assert_equal_i((int)data.size, (int)parser.header.pack_offset);

	git_bundle_parser_dispose(&parser);
	git_str_dispose(&data);
	git_str_dispose(&name);
}

void test_bundle_parse__rejects_bad_signature(void)
{
	assert_invalid("");
	assert_invalid("PACK\n");
	assert_invalid("# v1 git bundle\n\n");
	assert_invalid("# v4 git bundle\n\n");
	assert_invalid("# v2 git bundle extra\n\n");
	assert_invalid(" # v2 git bundle\n\n");

	/* a signature with no newline is truncated, not a bundle */
	assert_invalid("# v2 git bundle");
}

static int long_signature_read(
	void *payload, void *buf, size_t len, size_t *out_read)
{
	size_t *calls = payload;

	(*calls)++;

	if (*calls > 1) {
		git_error_set(GIT_ERROR_OS, "signature read was not bounded");
		return -1;
	}

	memset(buf, 'x', len);
	*out_read = len;
	return 0;
}

void test_bundle_parse__bounds_signature_reads(void)
{
	git_bundle_parser parser = GIT_BUNDLE_PARSER_INIT;
	size_t calls = 0;

	parser.read = long_signature_read;
	parser.payload = &calls;
	cl_assert_equal_i(GIT_EINVALID, git_bundle_parser_parse(&parser));

	/* Probing a non-bundle does not buffer its entire first line. */
	cl_assert_equal_i(1, (int)calls);

	git_bundle_parser_dispose(&parser);
}

static int endless_header_read(
	void *payload, void *buf, size_t len, size_t *out_read)
{
	size_t *calls = payload;
	const char signature[] = GIT_BUNDLE_SIGNATURE_V2 "\n";

	(*calls)++;

	if (*calls > 200) {
		git_error_set(GIT_ERROR_OS, "bundle header read was not bounded");
		return -1;
	}

	memset(buf, 'x', len);

	if (*calls == 1)
		memcpy(buf, signature, sizeof(signature) - 1);

	*out_read = len;
	return 0;
}

void test_bundle_parse__bounds_header_line_reads(void)
{
	git_bundle_parser parser = GIT_BUNDLE_PARSER_INIT;
	size_t calls = 0;

	parser.read = endless_header_read;
	parser.payload = &calls;

	cl_assert_equal_i(GIT_EINVALID, git_bundle_parser_parse(&parser));
	cl_assert(calls < 200);
	cl_assert(strstr(git_error_last()->message, "line is too long") != NULL);

	git_bundle_parser_dispose(&parser);
}

static int failing_read_header(
	size_t *len, git_object_t *type, git_odb_backend *backend,
	const git_oid *oid)
{
	GIT_UNUSED(len);
	GIT_UNUSED(type);
	GIT_UNUSED(backend);
	GIT_UNUSED(oid);

	git_error_set(GIT_ERROR_ODB, "injected object database failure");
	return GIT_EUSER;
}

static void failing_backend_free(git_odb_backend *backend)
{
	git__free(backend);
}

void test_bundle_parse__prerequisite_check_propagates_odb_errors(void)
{
	git_bundle_parser parser = GIT_BUNDLE_PARSER_INIT;
	git_odb_backend *backend;
	git_repository *repo;
	git_odb *odb;
	const char *data = "# v2 git bundle\n-" SHA1_A "\n\n";

	cl_git_pass(parse_memory(&parser, data, strlen(data)));
	cl_git_pass(git_repository_init(&repo, "prereq.git", true));
	cl_git_pass(git_repository_odb(&odb, repo));

	backend = git__calloc(1, sizeof(git_odb_backend));
	cl_assert(backend != NULL);
	backend->version = GIT_ODB_BACKEND_VERSION;
	backend->read_header = failing_read_header;
	backend->free = failing_backend_free;
	cl_git_pass(git_odb_add_backend(odb, backend, 100));

	cl_git_fail_with(GIT_EUSER,
		git_bundle_check_prerequisites(&parser.header, repo));
	cl_assert_equal_s("injected object database failure",
		git_error_last()->message);

	git_odb_free(odb);
	git_repository_free(repo);
	git_bundle_parser_dispose(&parser);
}

void test_bundle_parse__rejects_missing_separator(void)
{
	assert_invalid("# v2 git bundle\n" SHA1_A " refs/heads/master\n");
}

void test_bundle_parse__rejects_malformed_oid(void)
{
	assert_invalid("# v2 git bundle\nnotanoid refs/heads/master\n\n");
	assert_invalid("# v2 git bundle\n"
		"zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz refs/heads/master\n\n");
	assert_invalid("# v2 git bundle\n-notanoid\n\n");

	/* a SHA-256 id in a bundle that did not declare one */
	assert_invalid("# v2 git bundle\n" SHA256_A " refs/heads/master\n\n");
}

void test_bundle_parse__rejects_bad_ref_names(void)
{
	assert_invalid("# v2 git bundle\n" SHA1_A "\n\n");
	assert_invalid("# v2 git bundle\n" SHA1_A " \n\n");
	assert_invalid("# v2 git bundle\n" SHA1_A " refs/heads/\n\n");
	assert_invalid("# v2 git bundle\n" SHA1_A " refs/heads/a..b\n\n");
	assert_invalid("# v2 git bundle\n" SHA1_A " refs/heads/a b\n\n");
}

void test_bundle_parse__rejects_embedded_nul(void)
{
	git_bundle_parser parser = GIT_BUNDLE_PARSER_INIT;
	const char data[] =
		"# v2 git bundle\n"
		SHA1_A " refs/heads/mas\0ter\n"
		"\n";

	cl_assert_equal_i(GIT_EINVALID,
		parse_both(&parser, data, sizeof(data) - 1));
	cl_assert_equal_i(GIT_ERROR_INVALID, git_error_last()->klass);

	git_bundle_parser_dispose(&parser);
}

void test_bundle_parse__rejects_embedded_carriage_return(void)
{
	assert_invalid("# v2 git bundle\r\n\r\n");
	assert_invalid("# v2 git bundle\n" SHA1_A " refs/heads/master\r\n\n");
}

void test_bundle_parse__rejects_capabilities_in_v2(void)
{
	assert_invalid("# v2 git bundle\n@object-format=sha1\n\n");
}

void test_bundle_parse__rejects_malformed_capabilities(void)
{
	assert_invalid("# v3 git bundle\n@\n\n");
	assert_invalid("# v3 git bundle\n@=sha1\n\n");
	assert_invalid("# v3 git bundle\n@object-format\n\n");
	assert_invalid("# v3 git bundle\n@object-format=\n\n");

	/* conflicting declarations */
	assert_invalid("# v3 git bundle\n"
		"@object-format=sha1\n@object-format=sha256\n\n");
	assert_invalid("# v3 git bundle\n"
		"@object-format=sha1\n@object-format=sha1\n\n");

	/* a capability that follows a reference */
	assert_invalid("# v3 git bundle\n"
		SHA1_A " refs/heads/master\n@object-format=sha1\n\n");
}

void test_bundle_parse__rejects_unknown_capability(void)
{
	assert_unsupported("# v3 git bundle\n@nonsense\n"
		SHA1_A " refs/heads/master\n\n");
	assert_unsupported("# v3 git bundle\n@nonsense=1\n"
		SHA1_A " refs/heads/master\n\n");
}

void test_bundle_parse__rejects_filter(void)
{
	assert_unsupported("# v3 git bundle\n@filter=blob:none\n"
		SHA1_A " refs/heads/master\n\n");
}

void test_bundle_parse__rejects_unknown_object_format(void)
{
	assert_unsupported("# v3 git bundle\n@object-format=sha512\n\n");
}

/*
 * An unsupported capability must not short-circuit syntax checking: a
 * bundle that is both unsupported and malformed is reported as
 * malformed, so that transport probing does not select it.
 */
void test_bundle_parse__malformed_wins_over_unsupported(void)
{
	assert_invalid("# v3 git bundle\n@filter=blob:none\nnotanoid HEAD\n\n");
}

void test_bundle_parse__rejects_prerequisite_after_ref(void)
{
	assert_invalid("# v2 git bundle\n"
		SHA1_A " refs/heads/master\n-" SHA1_B "\n\n");
}

/* an operational failure is propagated, not reported as a format miss */
void test_bundle_parse__propagates_read_errors(void)
{
	git_bundle_parser parser = GIT_BUNDLE_PARSER_INIT;
	int fd;

	cl_git_mkfile(TMP_BUNDLE, V2_HEADER);

	/*
	 * A write-only descriptor fails every read.  A closed one would
	 * too, but the Windows debug runtime aborts the process instead of
	 * failing the call.
	 */
	cl_must_pass(fd = p_open(TMP_BUNDLE, O_WRONLY));

	cl_assert_equal_i(-1, git_bundle_parser_parse_fd(&parser, fd));
	cl_assert_equal_i(GIT_ERROR_OS, git_error_last()->klass);

	/* the failing source is released, and its error survives */
	assert_source_released(&parser);

	git_bundle_parser_dispose(&parser);
	p_close(fd);
}

void test_bundle_parse__parses_fixture_files(void)
{
	git_bundle_parser parser = GIT_BUNDLE_PARSER_INIT;
	git_remote_head *head;
	int fd;

	cl_must_pass(fd = p_open(cl_fixture("bundle/testrepo.bundle"), O_RDONLY));

	cl_git_pass(git_bundle_parser_parse_fd(&parser, fd));

	cl_assert_equal_i(2, parser.header.version);
	cl_assert_equal_i(GIT_OID_SHA1, parser.header.oid_type);
	cl_assert_equal_i(0, (int)parser.header.prerequisites.size);

	/* HEAD is recorded last, and is preserved in header order */
	head = git_vector_get(&parser.header.refs, parser.header.refs.length - 1);
	cl_assert_equal_s("HEAD", head->name);

	p_close(fd);

	cl_must_pass(fd = p_open(cl_fixture("bundle/incremental.bundle"), O_RDONLY));

	cl_git_pass(git_bundle_parser_parse_fd(&parser, fd));

	cl_assert_equal_i(1, (int)parser.header.prerequisites.size);
	cl_assert_equal_i(1, (int)parser.header.refs.length);

	p_close(fd);

	cl_must_pass(fd = p_open(cl_fixture("bundle/testrepo_256.bundle"), O_RDONLY));

	cl_git_pass(git_bundle_parser_parse_fd(&parser, fd));

	cl_assert_equal_i(3, parser.header.version);
	cl_assert_equal_i(GIT_OID_SHA256, parser.header.oid_type);

	git_bundle_parser_dispose(&parser);
	p_close(fd);
}

/* re-parsing replaces the previous result rather than adding to it */
void test_bundle_parse__reparse_releases_previous_result(void)
{
	git_bundle_parser parser = GIT_BUNDLE_PARSER_INIT;
	const char *first = V2_HEADER;
	const char *second =
		"# v2 git bundle\n" SHA1_B " refs/heads/only\n\n";
	git_remote_head *head;

	cl_git_pass(parse_memory(&parser, first, strlen(first)));
	cl_assert_equal_i(2, (int)parser.header.refs.length);
	cl_assert_equal_i(1, (int)parser.header.prerequisites.size);

	cl_git_pass(parse_memory(&parser, second, strlen(second)));
	cl_assert_equal_i(1, (int)parser.header.refs.length);
	cl_assert_equal_i(0, (int)parser.header.prerequisites.size);

	head = git_vector_get(&parser.header.refs, 0);
	cl_assert_equal_s("refs/heads/only", head->name);

	git_bundle_parser_dispose(&parser);
}

void test_bundle_parse__dispose_is_idempotent(void)
{
	git_bundle_parser parser = GIT_BUNDLE_PARSER_INIT;
	const char *data = V2_HEADER;

	/* a zero-initialized parser is valid */
	git_bundle_parser_dispose(&parser);

	cl_git_pass(parse_memory(&parser, data, strlen(data)));

	git_bundle_parser_dispose(&parser);
	git_bundle_parser_dispose(&parser);

	cl_assert_equal_i(0, (int)parser.header.version);
	cl_assert_equal_i(0, (int)parser.header.refs.length);
	cl_assert_equal_i(0, (int)parser.header.prerequisites.size);
}

/*
 * Whatever a parse returns, it hands the source back.  A successful and
 * an unsupported parse keep their header; a hard failure does not.
 */
void test_bundle_parse__releases_source_on_every_outcome(void)
{
	git_bundle_parser parser = GIT_BUNDLE_PARSER_INIT;
	const char *unsupported =
		"# v3 git bundle\n@filter=blob:none\n"
		SHA1_A " refs/heads/master\n\n";
	const char *invalid = "not a bundle\n";
	int fd;

	cl_git_mkfile(TMP_BUNDLE, V2_HEADER);
	cl_must_pass(fd = p_open(TMP_BUNDLE, O_RDONLY));

	cl_git_pass(git_bundle_parser_parse_fd(&parser, fd));
	assert_source_released(&parser);
	cl_assert_equal_i(2, (int)parser.header.version);

	/* the descriptor is borrowed: still open, still the caller's */
	cl_must_pass(p_lseek(fd, 0, SEEK_SET));
	p_close(fd);

	cl_assert_equal_i(GIT_ENOTSUPPORTED,
		parse_memory(&parser, unsupported, strlen(unsupported)));
	assert_source_released(&parser);
	cl_assert_equal_i(3, (int)parser.header.version);
	cl_assert_equal_i(1, (int)parser.header.refs.length);
	cl_assert(strstr(git_error_last()->message, "filtered") != NULL);

	cl_assert_equal_i(GIT_EINVALID,
		parse_memory(&parser, invalid, strlen(invalid)));
	assert_source_released(&parser);
	cl_assert_equal_i(0, (int)parser.header.version);
	cl_assert_equal_i(0, (int)parser.header.refs.length);
	cl_assert_equal_i(GIT_ERROR_INVALID, git_error_last()->klass);

	git_bundle_parser_dispose(&parser);
}
