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

/**
 * The parser's view of a bundle.  It reads through a callback rather
 * than from a descriptor so that the same code can parse a bundle that
 * exists only in memory.
 */
typedef struct {
	git_bundle_read_cb read;
	git_bundle_seek_cb seek; /**< optional */
	void *payload;

	/* state for the backings below; `payload` points at the reader */
	int fd;
	const unsigned char *data;
	size_t data_len;
	size_t data_pos;

	git_str buf;    /**< bytes read from the source, not yet consumed */
	size_t offset;  /**< source offset of the first unconsumed byte */
	unsigned int eof : 1;
} git_bundle_reader;

typedef struct {
	unsigned int version;
	git_oid_t oid_type;
	git_array_t(git_oid) prerequisites;
	git_vector refs;      /**< `git_remote_head *`, in header order */
	size_t pack_offset;   /**< offset of the first byte of the pack */
} git_bundle_header;

#define GIT_BUNDLE_HEADER_INIT { 0 }

/** Read the bundle from an open descriptor, starting at its current offset. */
void git_bundle_reader_fromfd(git_bundle_reader *reader, int fd);

/** Read the bundle from a buffer owned by the caller. */
void git_bundle_reader_frommemory(
	git_bundle_reader *reader, const void *data, size_t len);

void git_bundle_reader_dispose(git_bundle_reader *reader);

/**
 * Parse a bundle header.
 *
 * On success the header is populated and, when the reader supports
 * seeking, the source is repositioned at the first byte of the pack.
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
 * bundle that we cannot use".
 */
int git_bundle_header_parse(
	git_bundle_header *header, git_bundle_reader *reader);

void git_bundle_header_dispose(git_bundle_header *header);

#endif
