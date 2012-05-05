/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_odb_h__
#define INCLUDE_odb_h__

#include "git2/odb.h"
#include "git2/oid.h"
#include "git2/types.h"

#include "vector.h"
#include "cache.h"
#include "posix.h"

#define GIT_OBJECTS_DIR "objects/"
#define GIT_OBJECT_DIR_MODE 0777
#define GIT_OBJECT_FILE_MODE 0444

/* DO NOT EXPORT */
typedef struct {
	void *data;			/**< Raw, decompressed object data. */
	size_t len;			/**< Total number of bytes in data. */
	git_otype type;		/**< Type of this object. */
} git_rawobj;

/* EXPORT */
struct git_odb_object {
	git_cached_obj cached;
	git_rawobj raw;
};

/* EXPORT */
struct git_odb {
	git_refcount rc;
	git_vector backends;
	git_cache cache;
};

/*
 * Hash a git_rawobj internally.
 * The `git_rawobj` is supposed to be previously initialized
 */
int git_odb__hashobj(git_oid *id, git_rawobj *obj);

/*
 * Hash an open file descriptor.
 * This is a performance call when the contents of a fd need to be hashed,
 * but the fd is already open and we have the size of the contents.
 *
 * Saves us some `stat` calls.
 *
 * The fd is never closed, not even on error. It must be opened and closed
 * by the caller
 */
int git_odb__hashfd(git_oid *out, git_file fd, size_t size, git_otype type);

/*
 * Hash a `path`, assuming it could be a POSIX symlink: if the path is a symlink,
 * then the raw contents of the symlink will be hashed. Otherwise, this will
 * fallback to `git_odb__hashfd`.
 *
 * The hash type for this call is always `GIT_OBJ_BLOB` because symlinks may only
 * point to blobs.
 */
int git_odb__hashlink(git_oid *out, const char *path);

/*
 * Generate a GIT_ENOTFOUND error for the ODB.
 */
int git_odb__error_notfound(const char *message, const git_oid *oid);

/*
 * Generate a GIT_EAMBIGUOUS error for the ODB.
 */
int git_odb__error_ambiguous(const char *message);

#endif
