/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_odb_backend_h__
#define INCLUDE_git_odb_backend_h__

#include "common.h"
#include "types.h"
#include "oid.h"

/**
 * @file git2/backend.h
 * @brief Git custom backend functions
 * @defgroup git_backend Git custom backend API
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

typedef int (* git_odb_backend_read_cb)(
	void **, size_t *, git_otype *,
	struct git_odb_backend *,
	const git_oid *);

typedef int (* git_odb_backend_read_prefix_cb)(
	git_oid *,
	void **, size_t *, git_otype *,
	struct git_odb_backend *,
	const git_oid *,
	unsigned int);

typedef int (* git_odb_backend_read_header_cb)(
	size_t *, git_otype *,
	struct git_odb_backend *,
	const git_oid *);

typedef int (* git_odb_backend_write_cb)(
	git_oid *,
	struct git_odb_backend *,
	const void *,
	size_t,
	git_otype);

typedef int (* git_odb_backend_writestream_cb)(
	struct git_odb_stream **,
	struct git_odb_backend *,
	size_t,
	git_otype);

typedef int (* git_odb_backend_readstream_cb)(
	struct git_odb_stream **,
	struct git_odb_backend *,
	const git_oid *);

typedef int (* git_odb_backend_exists_cb)(
	struct git_odb_backend *,
	const git_oid *);

typedef void (* git_odb_backend_free_cb)(struct git_odb_backend *);

struct git_odb_stream;

/** An instance for a custom backend */
struct git_odb_backend {
	git_odb *odb;

	git_odb_backend_read_cb read;

	/* To find a unique object given a prefix
	 * of its oid.
	 * The oid given must be so that the
	 * remaining (GIT_OID_HEXSZ - len)*4 bits
	 * are 0s.
	 */
	git_odb_backend_read_prefix_cb read_prefix;

	git_odb_backend_read_header_cb read_header;

	git_odb_backend_write_cb write;

	git_odb_backend_writestream_cb writestream;

	git_odb_backend_readstream_cb readstream;

	git_odb_backend_exists_cb exists;

	git_odb_backend_free_cb free;
};

typedef int (*git_odb_stream_read_cb)(struct git_odb_stream *stream, char *buffer, size_t len);
typedef int (*git_odb_stream_write_cb)(struct git_odb_stream *stream, const char *buffer, size_t len);
typedef int (*git_odb_stream_finalize_write_cb)(git_oid *oid_p, struct git_odb_stream *stream);
typedef void (*git_odb_stream_free_cb)(struct git_odb_stream *stream);

/** A stream to read/write from a backend */
struct git_odb_stream {
	struct git_odb_backend *backend;
	int mode;

	git_odb_stream_read_cb read;
	git_odb_stream_write_cb write;
	git_odb_stream_finalize_write_cb finalize_write;
	git_odb_stream_free_cb free;
};

/** Streaming mode */
typedef enum {
	GIT_STREAM_RDONLY = (1 << 1),
	GIT_STREAM_WRONLY = (1 << 2),
	GIT_STREAM_RW = (GIT_STREAM_RDONLY | GIT_STREAM_WRONLY),
} git_odb_streammode;

GIT_EXTERN(int) git_odb_backend_pack(git_odb_backend **backend_out, const char *objects_dir);
GIT_EXTERN(int) git_odb_backend_loose(git_odb_backend **backend_out, const char *objects_dir, int compression_level, int do_fsync);

GIT_END_DECL

#endif
