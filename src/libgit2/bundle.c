/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "bundle.h"

#include "posix.h"
#include "odb.h"
#include "refs.h"
#include "repository.h"

#include "git2/odb.h"
#include "git2/refs.h"

#define BUNDLE_READ_CHUNK 8192
#define BUNDLE_HEADER_LINE_MAX (1024 * 1024)

#define BUNDLE_CAP_OBJECT_FORMAT "object-format"
#define BUNDLE_CAP_FILTER "filter"

static void bundle_header_dispose(git_bundle_header *header);

static int source_read_fd(void *payload, void *buf, size_t len, size_t *out_read)
{
	git_bundle_parser *parser = payload;
	ssize_t n;

	if ((n = p_read(parser->fd, buf, len)) < 0) {
		git_error_set(GIT_ERROR_OS, "failed to read bundle");
		return -1;
	}

	*out_read = (size_t)n;
	return 0;
}

static int source_seek_fd(void *payload, size_t offset)
{
	git_bundle_parser *parser = payload;

	if (p_lseek(parser->fd, (off_t)offset, SEEK_SET) < 0) {
		git_error_set(GIT_ERROR_OS, "failed to seek in bundle");
		return -1;
	}

	return 0;
}

static int source_read_memory(void *payload, void *buf, size_t len, size_t *out_read)
{
	git_bundle_parser *parser = payload;
	size_t remaining = parser->data_len - parser->data_pos;

	if (len > remaining)
		len = remaining;

	memcpy(buf, parser->data + parser->data_pos, len);
	parser->data_pos += len;

	*out_read = len;
	return 0;
}

static int source_seek_memory(void *payload, size_t offset)
{
	git_bundle_parser *parser = payload;

	parser->data_pos = min(offset, parser->data_len);
	return 0;
}

/*
 * Release the borrowed source and the transient parsing state, leaving
 * only the parsed header behind.  Nothing here sets an error, so a
 * caller can run this between a failed parse and its return without
 * disturbing the diagnosis.
 */
static void parser_source_clear(git_bundle_parser *parser)
{
	git_str_dispose(&parser->buf);

	parser->read = NULL;
	parser->seek = NULL;
	parser->payload = NULL;
	parser->fd = 0;
	parser->data = NULL;
	parser->data_len = 0;
	parser->data_pos = 0;
	parser->offset = 0;
	parser->eof = 0;
}

static int parser_fill(git_bundle_parser *parser)
{
	char chunk[BUNDLE_READ_CHUNK];
	size_t read_len = 0;
	int error;

	if ((error = parser->read(parser->payload, chunk, sizeof(chunk),
			&read_len)) < 0)
		return error;

	if (read_len == 0) {
		parser->eof = 1;
		return 0;
	}

	return git_str_put(&parser->buf, chunk, read_len);
}

/*
 * Read one line, without its newline, into `out`.  `max_len` may be
 * `SIZE_MAX` for no limit.  Returns GIT_ITEROVER at a clean end of input
 * and GIT_EINVALID for a truncated final line or for bytes that may not
 * appear in a header.
 */
static int parser_readline(
	git_str *out, git_bundle_parser *parser, size_t max_len)
{
	const char *nl = NULL;
	size_t linelen, scan_offset = 0;
	int error;

	git_str_clear(out);

	while (parser->buf.size == scan_offset ||
	       (nl = memchr(parser->buf.ptr + scan_offset, '\n',
			parser->buf.size - scan_offset)) == NULL) {
		if (parser->buf.size > max_len) {
			git_error_set(GIT_ERROR_INVALID,
				"bundle header line is too long");
			return GIT_EINVALID;
		}

		if (parser->eof) {
			if (parser->buf.size == 0)
				return GIT_ITEROVER;

			git_error_set(GIT_ERROR_INVALID,
				"truncated bundle header");
			return GIT_EINVALID;
		}

		scan_offset = parser->buf.size;

		if ((error = parser_fill(parser)) < 0)
			return error;
	}

	linelen = (size_t)(nl - parser->buf.ptr);

	if (linelen > max_len) {
		git_error_set(GIT_ERROR_INVALID,
			"bundle header line is too long");
		return GIT_EINVALID;
	}

	if (memchr(parser->buf.ptr, '\0', linelen) != NULL ||
	    memchr(parser->buf.ptr, '\r', linelen) != NULL) {
		git_error_set(GIT_ERROR_INVALID,
			"invalid character in bundle header");
		return GIT_EINVALID;
	}

	if ((error = git_str_put(out, parser->buf.ptr, linelen)) < 0)
		return error;

	git_str_consume_bytes(&parser->buf, linelen + 1);
	parser->offset += linelen + 1;

	return 0;
}

static int parse_oid(
	git_oid *out, const char **endptr, const char *line, git_oid_t oid_type)
{
	size_t hexsize = git_oid_hexsize(oid_type);

	if (strlen(line) < hexsize)
		goto invalid;

	if (git_oid_from_prefix(out, line, hexsize, oid_type) < 0)
		goto invalid;

	*endptr = line + hexsize;
	return 0;

invalid:
	git_error_set(GIT_ERROR_INVALID, "invalid object id in bundle header");
	return GIT_EINVALID;
}

/*
 * Record the first unsupported condition but keep parsing, so that a
 * caller probing an unknown file can tell a real bundle it cannot use
 * from a file that is not a bundle at all.  Returns true when the caller
 * should describe the condition.
 */
static bool note_unsupported(int *unsupported)
{
	if (*unsupported)
		return false;

	*unsupported = 1;
	return true;
}

static int parse_capability(
	git_bundle_header *header,
	const char *line,
	int *seen_object_format,
	int *unsupported)
{
	const char *value;
	size_t namelen;
	git_oid_t oid_type;

	/* skip the leading '@' */
	line++;

	value = strchr(line, '=');
	namelen = value ? (size_t)(value - line) : strlen(line);

	if (value)
		value++;

	if (namelen == 0) {
		git_error_set(GIT_ERROR_INVALID,
			"malformed bundle capability");
		return GIT_EINVALID;
	}

	if (namelen == CONST_STRLEN(BUNDLE_CAP_OBJECT_FORMAT) &&
	    memcmp(line, BUNDLE_CAP_OBJECT_FORMAT, namelen) == 0) {
		if (!value || !*value) {
			git_error_set(GIT_ERROR_INVALID,
				"bundle capability 'object-format' has no value");
			return GIT_EINVALID;
		}

		if (*seen_object_format) {
			git_error_set(GIT_ERROR_INVALID,
				"bundle declares 'object-format' more than once");
			return GIT_EINVALID;
		}

		*seen_object_format = 1;

		/*
		 * An object format we do not know fixes the width of every
		 * remaining object id, so there is nothing left to validate;
		 * report it immediately instead of noting it and parsing on.
		 */
		if ((oid_type = git_oid_type_fromstr(value)) == 0) {
			git_error_set(GIT_ERROR_INVALID,
				"unsupported bundle object format '%s'", value);
			return GIT_ENOTSUPPORTED;
		}

		header->oid_type = oid_type;
		return 0;
	}

	if (namelen == CONST_STRLEN(BUNDLE_CAP_FILTER) &&
	    memcmp(line, BUNDLE_CAP_FILTER, namelen) == 0) {
		if (note_unsupported(unsupported))
			git_error_set(GIT_ERROR_INVALID,
				"filtered bundles are not supported");

		return 0;
	}

	if (note_unsupported(unsupported))
		git_error_set(GIT_ERROR_INVALID,
			"unsupported bundle capability '%s'", line);

	return 0;
}

static int add_prerequisite(git_bundle_header *header, const char *line)
{
	git_oid oid, *slot;
	const char *rest;
	int error;

	/* skip the leading '-' */
	if ((error = parse_oid(&oid, &rest, line + 1, header->oid_type)) < 0)
		return error;

	/* the remainder is a human-readable comment; ignore it */
	if (*rest && *rest != ' ') {
		git_error_set(GIT_ERROR_INVALID,
			"invalid prerequisite in bundle header");
		return GIT_EINVALID;
	}

	slot = git_array_alloc(header->prerequisites);
	GIT_ERROR_CHECK_ALLOC(slot);

	git_oid_cpy(slot, &oid);

	return 0;
}

static int add_ref(git_bundle_header *header, const char *line)
{
	git_remote_head *head;
	const char *name;
	git_oid oid;
	int valid = 0, error;

	if ((error = parse_oid(&oid, &name, line, header->oid_type)) < 0)
		return error;

	if (*name != ' ' || !*(name + 1)) {
		git_error_set(GIT_ERROR_INVALID,
			"missing reference name in bundle header");
		return GIT_EINVALID;
	}

	name++;

	if ((error = git_reference_name_is_valid(&valid, name)) < 0)
		return error;

	if (!valid) {
		git_error_set(GIT_ERROR_INVALID,
			"invalid reference name '%s' in bundle header", name);
		return GIT_EINVALID;
	}

	head = git__calloc(1, sizeof(git_remote_head));
	GIT_ERROR_CHECK_ALLOC(head);

	head->name = git__strdup(name);

	if (!head->name) {
		git__free(head);
		return -1;
	}

	git_oid_cpy(&head->oid, &oid);

	if ((error = git_vector_insert(&header->refs, head)) < 0) {
		git__free(head->name);
		git__free(head);
		return error;
	}

	return 0;
}

static int parser_parse_header(git_bundle_parser *parser)
{
	git_bundle_header *header = &parser->header;
	git_str line = GIT_STR_INIT;
	int seen_object_format = 0, unsupported = 0;
	int error;

	memset(header, 0, sizeof(*header));
	header->oid_type = GIT_OID_SHA1;

	if ((error = git_vector_init(&header->refs, 0, NULL)) < 0)
		goto on_error;

	if ((error = parser_readline(&line, parser,
			max(CONST_STRLEN(GIT_BUNDLE_SIGNATURE_V2),
				CONST_STRLEN(GIT_BUNDLE_SIGNATURE_V3)))) < 0) {
		if (error == GIT_ITEROVER) {
			git_error_set(GIT_ERROR_INVALID, "not a bundle");
			error = GIT_EINVALID;
		}

		goto on_error;
	}

	if (strcmp(line.ptr, GIT_BUNDLE_SIGNATURE_V2) == 0) {
		header->version = 2;
	} else if (strcmp(line.ptr, GIT_BUNDLE_SIGNATURE_V3) == 0) {
		header->version = 3;
	} else {
		git_error_set(GIT_ERROR_INVALID, "not a bundle");
		error = GIT_EINVALID;
		goto on_error;
	}

	while ((error = parser_readline(&line, parser,
			BUNDLE_HEADER_LINE_MAX)) == 0) {
		if (line.size == 0)
			break;

		if (line.ptr[0] == '@') {
			if (header->version < 3) {
				git_error_set(GIT_ERROR_INVALID,
					"v2 bundles do not support capabilities");
				error = GIT_EINVALID;
				goto on_error;
			}

			if (header->refs.length ||
			    header->prerequisites.size) {
				git_error_set(GIT_ERROR_INVALID,
					"bundle capability follows a reference");
				error = GIT_EINVALID;
				goto on_error;
			}

			if ((error = parse_capability(header, line.ptr,
					&seen_object_format, &unsupported)) < 0)
				goto on_error;

			continue;
		}

		if (line.ptr[0] == '-') {
			if (header->refs.length) {
				git_error_set(GIT_ERROR_INVALID,
					"bundle prerequisite follows a reference");
				error = GIT_EINVALID;
				goto on_error;
			}

			if ((error = add_prerequisite(header, line.ptr)) < 0)
				goto on_error;

			continue;
		}

		if ((error = add_ref(header, line.ptr)) < 0)
			goto on_error;
	}

	if (error == GIT_ITEROVER) {
		git_error_set(GIT_ERROR_INVALID,
			"bundle header has no separator");
		error = GIT_EINVALID;
	}

	if (error < 0)
		goto on_error;

	header->pack_offset = parser->offset;

	if (parser->seek &&
	    (error = parser->seek(parser->payload, header->pack_offset)) < 0)
		goto on_error;

	git_str_dispose(&line);

	/* the error describing the unsupported condition is already set */
	return unsupported ? GIT_ENOTSUPPORTED : 0;

on_error:
	git_str_dispose(&line);

	if (error != GIT_ENOTSUPPORTED)
		bundle_header_dispose(header);

	return error;
}

int git_bundle_parser_parse(git_bundle_parser *parser)
{
	int error;

	GIT_ASSERT_ARG(parser);
	GIT_ASSERT_ARG(parser->read);

	bundle_header_dispose(&parser->header);

	error = parser_parse_header(parser);

	parser_source_clear(parser);

	return error;
}

int git_bundle_parser_parse_fd(git_bundle_parser *parser, int fd)
{
	GIT_ASSERT_ARG(parser);

	parser->read = source_read_fd;
	parser->seek = source_seek_fd;
	parser->payload = parser;
	parser->fd = fd;

	return git_bundle_parser_parse(parser);
}

int git_bundle_parser_parse_memory(
	git_bundle_parser *parser, const void *data, size_t len)
{
	GIT_ASSERT_ARG(parser);

	parser->read = source_read_memory;
	parser->seek = source_seek_memory;
	parser->payload = parser;
	parser->data = data;
	parser->data_len = len;
	parser->data_pos = 0;

	return git_bundle_parser_parse(parser);
}

void git_bundle_parser_dispose(git_bundle_parser *parser)
{
	if (!parser)
		return;

	parser_source_clear(parser);
	bundle_header_dispose(&parser->header);
}

int git_bundle_check_prerequisites(
	git_bundle_header *header, git_repository *repo)
{
	git_odb *odb;
	size_t i;
	int error;

	GIT_ASSERT_ARG(header);
	GIT_ASSERT_ARG(repo);

	if (!header->prerequisites.size)
		return 0;

	if ((error = git_repository_odb__weakptr(&odb, repo)) < 0)
		return error;

	for (i = 0; i < header->prerequisites.size; i++) {
		const git_oid *oid = &header->prerequisites.ptr[i];
		char str[GIT_OID_MAX_HEXSIZE + 1];
		size_t size;
		git_object_t type;

		error = git_odb_read_header(&size, &type, odb, oid);

		if (error == 0)
			continue;
		else if (error != GIT_ENOTFOUND)
			return error;

		git_oid_tostr(str, sizeof(str), oid);
		git_error_set(GIT_ERROR_ODB,
			"the bundle requires object %s, which is missing from the repository",
			str);

		return -1;
	}

	return 0;
}

static void bundle_header_dispose(git_bundle_header *header)
{
	git_remote_head *head;
	size_t i;

	git_vector_foreach(&header->refs, i, head) {
		git__free(head->name);
		git__free(head);
	}

	git_vector_dispose(&header->refs);
	git_array_clear(header->prerequisites);

	memset(header, 0, sizeof(*header));
}
