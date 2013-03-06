/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_refdb_h__
#define INCLUDE_refdb_h__

#include "git2/refdb.h"
#include "repository.h"

struct git_refdb {
	git_refcount rc;
	git_repository *repo;
	git_refdb_backend *backend;
};

int git_refdb_exists(
	int *exists,
	git_refdb *refdb,
	const char *ref_name);

int git_refdb_lookup(git_refdb *refdb, git_reference *ref);

int git_refdb_foreach(
	git_refdb *refdb,
	unsigned int list_flags,
	git_reference_foreach_cb callback,
	void *payload);

int git_refdb_foreach_glob(
	git_refdb *refdb,
	const char *glob,
	unsigned int list_flags,
	git_reference_foreach_cb callback,
	void *payload);

int git_refdb_write(git_refdb *refdb, git_reference *);

int git_refdb_delete(struct git_refdb *refdb, const char *ref_name);

int git_refdb_compress(struct git_refdb *refdb);

void git_refdb_free(git_refdb *refdb);

#endif
