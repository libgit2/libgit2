/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "posix.h"

#include "git2/object.h"
#include "git2/refs.h"
#include "git2/refdb.h"
#include "git2/sys/refdb_backend.h"

#include "hash.h"
#include "refdb.h"
#include "refs.h"

int git_refdb_new(git_refdb **out, git_repository *repo)
{
	git_refdb *db;

	assert(out && repo);

	db = git__calloc(1, sizeof(*db));
	GITERR_CHECK_ALLOC(db);

	db->repo = repo;

	*out = db;
	GIT_REFCOUNT_INC(db);
	return 0;
}

int git_refdb_open(git_refdb **out, git_repository *repo)
{
	git_refdb *db;
	git_refdb_backend *dir;

	assert(out && repo);

	*out = NULL;

	if (git_refdb_new(&db, repo) < 0)
		return -1;

	/* Add the default (filesystem) backend */
	if (git_refdb_backend_fs(&dir, repo) < 0) {
		git_refdb_free(db);
		return -1;
	}

	db->repo = repo;
	db->backend = dir;

	*out = db;
	return 0;
}

static void refdb_free_backend(git_refdb *db)
{
	if (db->backend) {
		if (db->backend->free)
			db->backend->free(db->backend);
		else
			git__free(db->backend);
	}
}

int git_refdb_set_backend(git_refdb *db, git_refdb_backend *backend)
{
	refdb_free_backend(db);
	db->backend = backend;

	return 0;
}

int git_refdb_compress(git_refdb *db)
{
	assert(db);

	if (db->backend->compress)
		return db->backend->compress(db->backend);

	return 0;
}

static void refdb_free(git_refdb *db)
{
	refdb_free_backend(db);
	git__free(db);
}

void git_refdb_free(git_refdb *db)
{
	if (db == NULL)
		return;

	GIT_REFCOUNT_DEC(db, refdb_free);
}

int git_refdb_exists(int *exists, git_refdb *refdb, const char *ref_name)
{
	assert(exists && refdb && refdb->backend);

	return refdb->backend->exists(exists, refdb->backend, ref_name);
}

int git_refdb_lookup(git_reference **out, git_refdb *db, const char *ref_name)
{
	git_reference *ref;
	int error;

	assert(db && db->backend && out && ref_name);

	if (!(error = db->backend->lookup(&ref, db->backend, ref_name))) {
		ref->db = db;
		*out = ref;
	} else {
		*out = NULL;
	}

	return error;
}

int git_refdb_iterator(git_reference_iterator **out, git_refdb *db)
{
	if (!db->backend || !db->backend->iterator) {
		giterr_set(GITERR_REFERENCE, "This backend doesn't support iterators");
		return -1;
	}

	if (db->backend->iterator(out, db->backend) < 0)
		return -1;

	return 0;
}

int git_refdb_iterator_glob(git_reference_iterator **out, git_refdb *db, const char *glob)
{
	if (!db->backend) {
		giterr_set(GITERR_REFERENCE, "There are no backends loaded");
		return -1;
	}

	if (db->backend->iterator_glob)
		return db->backend->iterator_glob(out, db->backend, glob);

	/* If the backend doesn't support glob-filtering themselves, we have to do it */
	if (db->backend->iterator(out, db->backend) < 0)
		return -1;

	(*out)->glob = git__strdup(glob);
	if (!(*out)->glob) {
		db->backend->iterator_free(*out);
		return -1;
	}

	return 0;
}

int git_refdb_next(const char **out, git_reference_iterator *iter)
{
	int error;

	if (!iter->glob)
		return iter->backend->next(out, iter);

	/* If the iterator has a glob, we need to filter */
	while ((error = iter->backend->next(out, iter)) == 0) {
		if (!p_fnmatch(iter->glob, *out, 0))
			break;
	}

	return error;
}

void git_refdb_iterator_free(git_reference_iterator *iter)
{
	git__free(iter->glob);
	iter->backend->iterator_free(iter);
}

struct glob_cb_data {
	const char *glob;
	git_reference_foreach_cb callback;
	void *payload;
};

int git_refdb_write(git_refdb *db, const git_reference *ref)
{
	assert(db && db->backend);

	return db->backend->write(db->backend, ref);
}

int git_refdb_delete(struct git_refdb *db, const git_reference *ref)
{
	assert(db && db->backend);

	return db->backend->delete(db->backend, ref);
}
