/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_bundle_h__
#define INCLUDE_bundle_h__

#include "common.h"

#include "array.h"
#include "oid.h"
#include "str.h"
#include "vector.h"

#include "git2/remote.h"
#include "git2/types.h"

#define GIT_BUNDLE_SIGNATURE_V2 "# v2 git bundle"
#define GIT_BUNDLE_SIGNATURE_V3 "# v3 git bundle"

/**
 * Read up to `len` bytes from the bundle source.  A short read is
 * permitted; `*out_read` is set to zero at end of input.
 */
typedef int GIT_CALLBACK(git_bundle_read_cb)(
	void *payload, void *buf, size_t len, size_t *out_read);

/** Reposition the bundle source at the given absolute offset. */
typedef int GIT_CALLBACK(git_bundle_seek_cb)(void *payload, size_t offset);

/** A parsed bundle header, owned by the parser that produced it. */
typedef struct {
	unsigned int version;
	git_oid_t oid_type;
	git_array_t(git_oid) prerequisites;
	git_vector refs;      /**< `git_remote_head *`, in header order */
	size_t pack_offset;   /**< offset of the first byte of the pack */
} git_bundle_header;

/**
 * A bundle parser and the header it has parsed.
 *
 * The parser reads through a callback rather than from a descriptor so
 * that the same code can parse a bundle that exists only in memory.  It
 * borrows its source: descriptors, buffers, and callback payloads all
 * remain the caller's to close or free, and the parser releases its hold
 * on them before every parse returns.  What it does own is the parsed
 * header, which stays valid until the next parse or `dispose`.
 *
 * A zero-initialized parser is valid.
 */
typedef struct {
	/* borrowed source and transient parsing state */
	git_bundle_read_cb read;
	git_bundle_seek_cb seek; /**< optional */
	void *payload;

	/* state for the backings below; `payload` points at the parser */
	int fd;
	const unsigned char *data;
	size_t data_len;
	size_t data_pos;

	git_str buf;    /**< bytes read from the source, not yet consumed */
	size_t offset;  /**< source offset of the first unconsumed byte */
	unsigned int eof : 1;

	/* parsed result, owned by the parser */
	git_bundle_header header;
} git_bundle_parser;

#define GIT_BUNDLE_PARSER_INIT { 0 }

/**
 * Parse a bundle header from the source that the caller has installed in
 * `read`, `seek`, and `payload`.
 *
 * Any previously parsed header is released first.  On success the header
 * is populated and, when the source supports seeking, it is repositioned
 * at the first byte of the pack.  Whatever the outcome, the source and
 * the parser's transient read buffer are released before returning.
 *
 * Returns 0 for a bundle this build can use, `GIT_EINVALID` when the
 * input is not a bundle or its header is malformed, `GIT_ENOTSUPPORTED`
 * for a well-formed bundle whose semantics are not supported (a filtered
 * bundle, an unknown v3 capability, or an object format this build does
 * not have), and any other negative value for an operational failure
 * such as a read or allocation error.
 *
 * A `GIT_ENOTSUPPORTED` result is only returned once the rest of the
 * header has been validated, so callers can rely on it meaning "a real
 * bundle that we cannot use"; its header is parsed and retained.  A hard
 * failure releases the partial result.
 */
int git_bundle_parser_parse(git_bundle_parser *parser);

/**
 * Parse the bundle on an open descriptor, starting at its current
 * offset.  The descriptor is borrowed and is never closed.
 */
int git_bundle_parser_parse_fd(git_bundle_parser *parser, int fd);

/** Parse the bundle in a buffer owned by the caller. */
int git_bundle_parser_parse_memory(
	git_bundle_parser *parser, const void *data, size_t len);

/** Release the parsed header.  Disposing twice is harmless. */
void git_bundle_parser_dispose(git_bundle_parser *parser);

/**
 * Require every prerequisite object to exist in `repo`.
 *
 * This is what Git checks when it unbundles: existence, not full
 * reachability.  Thin-pack resolution against the destination object
 * database is the backstop for a delta base that is genuinely missing.
 */
int git_bundle_check_prerequisites(
	git_bundle_header *header, git_repository *repo);

/**
 * Decide whether `url` names a bundle file, by parsing its header.
 *
 * A path that is not a local path, does not exist, or does not name a
 * regular file is not a bundle, and neither is a regular file whose
 * header is not a bundle header; those cases set `*out` to false and
 * return 0 with no error left set.  A well-formed bundle whose semantics
 * this build does not support is still a bundle, so that `connect` can
 * report the specific reason rather than an unsupported URL protocol.
 * Allocation, open, and read failures after a path has been classified
 * as an existing regular file are returned as negative values.
 *
 * Implemented by the bundle transport.
 */
int git_transport_bundle__probe(bool *out, const char *url);

#endif
