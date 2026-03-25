/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_bundle_h__
#define INCLUDE_bundle_h__

#include "common.h"
#include "str.h"
#include "vector.h"

#include "git2/net.h"
#include "git2/oid.h"
#include "git2/types.h"

/** Maximum bytes to read when parsing the bundle header (1 MiB) */
#define GIT_BUNDLE_MAX_HEADER_SIZE (1024 * 1024)

/** Signature for bundle v2 files */
#define GIT_BUNDLE_SIGNATURE_V2 "# v2 git bundle\n"
/** Signature for bundle v3 files */
#define GIT_BUNDLE_SIGNATURE_V3 "# v3 git bundle\n"

/** Bitmask values for parsed v3 capabilities */
#define GIT_BUNDLE_CAP_OBJECT_FORMAT (1u << 0)

/**
 * Internal representation of a parsed git bundle.
 *
 * Invariants:
 *  - All strings in `refs` are heap-allocated copies; none point into any
 *    transient parse buffer.
 *  - All git_oid values in `prerequisites` are copies.
 *  - `pack_start_offset` is the exact byte position where the packfile
 *    begins in the file named by `path`.
 */
struct git_bundle {
	unsigned int version;         /* bundle format version: 2 or 3 */
	git_oid_t oid_type;           /* GIT_OID_SHA1 or GIT_OID_SHA256 */
	unsigned int capabilities;    /* bitmask of parsed v3 capabilities */
	git_vector prerequisites;     /* owned git_oid* entries */
	git_vector refs;              /* owned git_remote_head* entries */
	git_str path;                 /* path to the bundle file */
	size_t pack_start_offset;     /* byte offset of first pack byte */
	size_t header_len;            /* number of header bytes (for validation) */
};

#endif
