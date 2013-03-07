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
#include "hash.h"
#include "refdb.h"
#include "refs.h"

#include "git2/refdb_backend.h"

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
	if (git_refdb_backend_fs(&dir, repo, db) < 0) {
		git_refdb_free(db);
		return -1;
	}

	db->repo = repo;
	db->backend = dir;

	*out = db;
	return 0;
}

int git_refdb_set_backend(git_refdb *db, git_refdb_backend *backend)
{
	if (db->backend) {
		if(db->backend->free)
			db->backend->free(db->backend);
		else
			git__free(db->backend);
	}

	db->backend = backend;

	return 0;
}

int git_refdb_compress(git_refdb *db)
{
	assert(db);
	
	if (db->backend->compress) {
		return db->backend->compress(db->backend);
	}
	
	return 0;
}

void git_refdb_free(git_refdb *db)
{
	if (db->backend) {
		if(db->backend->free)
			db->backend->free(db->backend);
		else
			git__free(db->backend);
	}

	git__free(db);
}

int git_refdb_exists(int *exists, git_refdb *refdb, const char *ref_name)
{
	assert(exists && refdb && refdb->backend);

	return refdb->backend->exists(exists, refdb->backend, ref_name);
}

int git_refdb_lookup(git_reference **out, git_refdb *db, const char *ref_name)
{
	assert(db && db->backend && ref_name);

	return db->backend->lookup(out, db->backend, ref_name);
}

int git_refdb_foreach(
	git_refdb *db,
	unsigned int list_flags,
	git_reference_foreach_cb callback,
	void *payload)
{
	assert(db && db->backend);

	return db->backend->foreach(db->backend, list_flags, callback, payload);
}

struct glob_cb_data {
	const char *glob;
	git_reference_foreach_cb callback;
	void *payload;
};

static int fromglob_cb(const char *reference_name, void *payload)
{
	struct glob_cb_data *data = (struct glob_cb_data *)payload;

	if (!p_fnmatch(data->glob, reference_name, 0))
		return data->callback(reference_name, data->payload);

	return 0;
}

int git_refdb_foreach_glob(
	git_refdb *db,
	const char *glob,
	unsigned int list_flags,
	git_reference_foreach_cb callback,
	void *payload)
{
	int error;
	struct glob_cb_data data;

	assert(db && db->backend && glob && callback);

	if(db->backend->foreach_glob != NULL)
		error = db->backend->foreach_glob(db->backend,
			glob, list_flags, callback, payload);
	else {
		data.glob = glob;
		data.callback = callback;
		data.payload = payload;

		error = db->backend->foreach(db->backend,
			list_flags, fromglob_cb, &data);
	}

	return error;
}

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
