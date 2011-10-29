/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_filebuf_h__
#define INCLUDE_filebuf_h__

#include "fileops.h"
#include "hash.h"
#include "git2/zlib.h"

#ifdef GIT_THREADS
#	define GIT_FILEBUF_THREADS
#endif

#define GIT_FILEBUF_HASH_CONTENTS		(1 << 0)
#define GIT_FILEBUF_APPEND				(1 << 2)
#define GIT_FILEBUF_FORCE				(1 << 3)
#define GIT_FILEBUF_TEMPORARY			(1 << 4)
#define GIT_FILEBUF_DEFLATE_SHIFT		(5)

#define GIT_FILELOCK_EXTENSION ".lock\0"
#define GIT_FILELOCK_EXTLENGTH 6

struct git_filebuf {
	char *path_original;
	char *path_lock;

	int (*write)(struct git_filebuf *file, void *source, size_t len);

	git_hash_ctx *digest;

	unsigned char *buffer;
	unsigned char *z_buf;

	z_stream zs;
	int flush_mode;

	size_t buf_size, buf_pos;
	git_file fd;
};

typedef struct git_filebuf git_filebuf;

int git_filebuf_write(git_filebuf *lock, const void *buff, size_t len);
int git_filebuf_reserve(git_filebuf *file, void **buff, size_t len);
int git_filebuf_printf(git_filebuf *file, const char *format, ...) GIT_FORMAT_PRINTF(2, 3);

int git_filebuf_open(git_filebuf *lock, const char *path, int flags);
int git_filebuf_commit(git_filebuf *lock, mode_t mode);
int git_filebuf_commit_at(git_filebuf *lock, const char *path, mode_t mode);
void git_filebuf_cleanup(git_filebuf *lock);
int git_filebuf_hash(git_oid *oid, git_filebuf *file);

#endif
