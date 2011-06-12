/*
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2,
 * as published by the Free Software Foundation.
 *
 * In addition to the permissions in the GNU General Public License,
 * the authors give you unlimited permission to link the compiled
 * version of this file into combinations with other programs,
 * and to distribute those combinations without any restriction
 * coming from the use of this file.  (The General Public License
 * restrictions do apply in other respects; for example, they cover
 * modification of the file, and distribution when not linked into
 * a combined executable.)
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
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

struct git_odb_stream;

/** An instance for a custom backend */
struct git_odb_backend {
	git_odb *odb;

	int (* read)(
			void **, size_t *, git_otype *,
			struct git_odb_backend *,
			const git_oid *);

	/* To find a unique object given a prefix
	 * of its oid.
	 * The oid given must be so that the
	 * remaining (GIT_OID_HEXSZ - len)*4 bits
	 * are 0s.
	 */
	int (* read_prefix)(
			git_oid *,
			void **, size_t *, git_otype *,
			struct git_odb_backend *,
			const git_oid *,
			unsigned int);

	int (* read_header)(
			size_t *, git_otype *,
			struct git_odb_backend *,
			const git_oid *);

	int (* write)(
			git_oid *,
			struct git_odb_backend *,
			const void *,
			size_t,
			git_otype);

	int (* writestream)(
			struct git_odb_stream **,
			struct git_odb_backend *,
			size_t,
			git_otype);

	int (* readstream)(
			struct git_odb_stream **,
			struct git_odb_backend *,
			const git_oid *);

	int (* exists)(
			struct git_odb_backend *,
			const git_oid *);

	void (* free)(struct git_odb_backend *);
};

/** A stream to read/write from a backend */
struct git_odb_stream {
	struct git_odb_backend *backend;
	int mode;

	int (*read)(struct git_odb_stream *stream, char *buffer, size_t len);
	int (*write)(struct git_odb_stream *stream, const char *buffer, size_t len);
	int (*finalize_write)(git_oid *oid_p, struct git_odb_stream *stream);
	void (*free)(struct git_odb_stream *stream);
};

/** Streaming mode */
typedef enum {
	GIT_STREAM_RDONLY = (1 << 1),
	GIT_STREAM_WRONLY = (1 << 2),
	GIT_STREAM_RW = (GIT_STREAM_RDONLY | GIT_STREAM_WRONLY),
} git_odb_streammode;

GIT_EXTERN(int) git_odb_backend_pack(git_odb_backend **backend_out, const char *objects_dir);
GIT_EXTERN(int) git_odb_backend_loose(git_odb_backend **backend_out, const char *objects_dir);

GIT_END_DECL

#endif
