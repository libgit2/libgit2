/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "bundle.h"
#include "futils.h"
#include "parse.h"
#include "repository.h"
#include "git2/odb_backend.h"

#define GIT_BUNDLE_V2_SIGNATURE "# v2 git bundle\n"
#define GIT_BUNDLE_V3_SIGNATURE "# v3 git bundle\n"

/**
 * A bundle file is made up of a header plus a packfile, separated by two
 * newlines.
 */
static int read_until_packfile(git_str *str, git_file fd)
{
	ssize_t bytes_read = 0;
	char buf[1];

	while ((bytes_read = p_read(fd, buf, 1)) == 1) {
		if (buf[0] == '\n' && git_str_len(str) > 0 &&
		    git_str_cstr(str)[git_str_len(str) - 1] == '\n') {
			return 0;
		}

		if (git_str_putc(str, buf[0]) < 0) {
			return -1;
		}
	}

	return 0;
}

static int read_bundle_version(git_bundle_header *header, git_file fd)
{
	int error = 0;
	git_str version = GIT_STR_INIT;

	if ((error = git_futils_readbuffer_fd(&version, fd, 16)) < 0) {
		goto cleanup;
	}

	if (git__strncmp(git_str_cstr(&version), GIT_BUNDLE_V2_SIGNATURE, 16) ==
	    0) {
		header->version = 2;
	} else {
		if (git__strncmp(
		            git_str_cstr(&version), GIT_BUNDLE_V3_SIGNATURE,
		            16) == 0) {
			header->version = 3;
		} else {
			error = GIT_EINVALID;
		}
	}

cleanup:
	git_str_dispose(&version);
	return error;
}

static int
parse_bundle_capabilities(git_bundle_header *header, git_parse_ctx *parser)
{
	int error = 0;

	git_parse_advance_chars(parser, 1);

	if (git_parse_ctx_contains(parser, "object-format=", 14)) {
		git_parse_advance_chars(parser, 14);
		if (git_parse_ctx_contains(parser, "sha1", 4)) {
			header->oid_type = GIT_OID_SHA1;
		}
		if (git_parse_ctx_contains(parser, "sha256", 6)) {
#ifdef GIT_EXPERIMENTAL_SHA256
			header->oid_type = GIT_OID_SHA256;
#else
			error = GIT_ENOTSUPPORTED;
#endif
		}
		goto cleanup;
	}

	if (git_parse_ctx_contains(parser, "filter=", 7)) {
		error = GIT_ENOTSUPPORTED;
		goto cleanup;
	}

	error = GIT_EINVALID;

cleanup:
	return error;
}

static int
parse_bundle_prerequisites(git_bundle_header *header, git_parse_ctx *parser)
{
	int error = 0;
	git_str name = GIT_STR_INIT;
	git_oid *oid;

	oid = git__calloc(1, sizeof(git_oid));
	GIT_ERROR_CHECK_ALLOC(oid);

	git_parse_advance_chars(parser, 1);
	if ((error = git_parse_advance_oid(oid, parser, header->oid_type)) <
	            0 ||
	    (error = git_vector_insert(&header->prerequisites, oid)) < 0) {
		git__free(oid);
		goto cleanup;
	};

cleanup:
	git_str_dispose(&name);
	return error;
}

static int
parse_bundle_references(git_bundle_header *header, git_parse_ctx *parser)
{
	int error = 0;
	git_str name = GIT_STR_INIT;
	git_remote_head *head;

	head = git__calloc(1, sizeof(git_remote_head));
	GIT_ERROR_CHECK_ALLOC(head);

	if ((error = git_parse_advance_oid(
	             &head->oid, parser, header->oid_type)) < 0 ||
	    (error = git_parse_advance_ws(parser)) < 0 ||
	    (error = git_str_set(&name, parser->line, parser->line_len)) < 0) {
		goto cleanup;
	};

	git_str_rtrim(&name);
	head->name = git_str_detach(&name);
	git_vector_insert(&header->refs, head);

cleanup:
	git_str_dispose(&name);
	return error;
}

static int parse_bundle_header(git_bundle_header *header, git_str *buf)
{
	int error = 0;
	git_parse_ctx parser;
	char c;

	if ((error = git_parse_ctx_init(
	             &parser, git_str_cstr(buf), git_str_len(buf))) < 0)
		goto cleanup;

	for (; parser.remain_len; git_parse_advance_line(&parser)) {
		if ((error = git_parse_peek(
		             &c, &parser, GIT_PARSE_PEEK_SKIP_WHITESPACE)) <
		    0) {
			goto cleanup;
		}

		if (header->version == 3 && c == '@') {
			if ((error = parse_bundle_capabilities(
			             header, &parser)) < 0) {
				goto cleanup;
			}
			continue;
		}

		if (c == '-') {
			if ((error = parse_bundle_prerequisites(
			             header, &parser)) < 0) {
				goto cleanup;
			}
			continue;
		}

		if ((error = parse_bundle_references(header, &parser)) < 0) {
			goto cleanup;
		}
	}

cleanup:
	return error;
}

int git_bundle_header_open(git_bundle_header **out, const char *url)
{
	int error = 0;
	int fd = 0;
	git_bundle_header *header = NULL;
	git_str buf = GIT_STR_INIT;

	if ((fd = git_futils_open_ro(url)) < 0) {
		git_str_dispose(&buf);
		return fd;
	}

	header = git__calloc(1, sizeof(git_bundle_header));
	GIT_ERROR_CHECK_ALLOC(header);
	header->version = 0;
	header->oid_type = GIT_OID_SHA1;
	if ((error = git_vector_init(&header->refs, 0, NULL)) < 0 ||
	    (error = git_vector_init(&header->prerequisites, 0, NULL)) < 0 ||
	    (error = read_bundle_version(header, fd)) < 0 ||
	    (error = read_until_packfile(&buf, fd)) < 0 ||
	    (error = parse_bundle_header(header, &buf)) < 0) {
		git_bundle_header_free(header);
		goto cleanup;
	}

	*out = header;

cleanup:
	git_str_dispose(&buf);
	p_close(fd);
	return error;
}

void git_bundle_header_free(git_bundle_header *bundle)
{
	size_t i;
	git_remote_head *ref;
	git_oid *prerequisites;

	if (!bundle) {
		return;
	}

	git_vector_foreach (&bundle->refs, i, ref) {
		git__free(ref->name);
		git__free(ref->symref_target);
		git__free(ref);
	}
	git_vector_dispose(&bundle->refs);

	i = 0;
	git_vector_foreach (&bundle->prerequisites, i, prerequisites) {
		git__free(prerequisites);
	}
	git_vector_dispose(&bundle->prerequisites);

	git__free(bundle);
}

int git_bundle__is_bundle(const char *url)
{
	int error = 0;
	git_bundle_header *header = NULL;

	if ((error = git_bundle_header_open(&header, url)) < 0) {
		return 0;
	}

	git_bundle_header_free(header);
	return 1;
}

int git_bundle__read_pack(
        git_repository *repo,
        const char *url,
        git_indexer_progress *stats)
{
	int error = 0;
	int fd = 0;
	git_str buf = GIT_STR_INIT;
	git_odb *odb;
	ssize_t bytes_read = 0;
	char buffer[1024];
	struct git_odb_writepack *writepack = NULL;

	if ((fd = git_futils_open_ro(url)) < 0) {
		git_str_dispose(&buf);
		return fd;
	}

	if ((error = read_until_packfile(&buf, fd)) < 0 ||
	    (error = git_repository_odb__weakptr(&odb, repo)) < 0 ||
	    (error = git_odb_write_pack(&writepack, odb, NULL, NULL)) != 0) {
		goto cleanup;
	}

	while ((bytes_read = p_read(fd, buffer, 1024)) > 0) {
		if ((error = writepack->append(
		             writepack, buffer, bytes_read, stats)) < 0)
			goto cleanup;
	}

	if ((error = writepack->commit(writepack, stats)) < 0)
		goto cleanup;

cleanup:
	if (writepack)
		writepack->free(writepack);
	git_str_dispose(&buf);
	p_close(fd);
	return error;
}
