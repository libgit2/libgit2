/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_repository_h__
#define INCLUDE_repository_h__

#include "git2/common.h"
#include "git2/oid.h"
#include "git2/odb.h"
#include "git2/repository.h"
#include "git2/object.h"

#include "hashtable.h"
#include "index.h"
#include "cache.h"
#include "refs.h"
#include "buffer.h"
#include "odb.h"

#define DOT_GIT ".git"
#define GIT_DIR DOT_GIT "/"
#define GIT_DIR_MODE 0755
#define GIT_BARE_DIR_MODE 0777

struct git_object {
	git_cached_obj cached;
	git_repository *repo;
	git_otype type;
};

struct git_repository {
	git_odb *db;

	git_cache objects;
	git_refcache references;

	char *path_repository;
	char *path_index;
	char *path_odb;
	char *path_workdir;

	unsigned is_bare:1;
	unsigned int lru_counter;
};

/* fully free the object; internal method, do not
 * export */
void git_object__free(void *object);

int git_oid__parse(git_oid *oid, const char **buffer_out, const char *buffer_end, const char *header);
void git_oid__writebuf(git_buf *buf, const char *header, const git_oid *oid);

#endif
