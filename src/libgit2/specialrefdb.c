/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "specialrefdb.h"

#include "git2/sys/specialrefdb_backend.h"

int git_specialrefdb_new(git_specialrefdb **out, git_repository *repo)
{
	git_specialrefdb *db;

	GIT_ASSERT_ARG(out);
	GIT_ASSERT_ARG(repo);

	db = git__calloc(1, sizeof(*db));
	GIT_ERROR_CHECK_ALLOC(db);

	db->repo = repo;

	*out = db;
	GIT_REFCOUNT_INC(db);
	return 0;
}

int git_specialrefdb_open(git_specialrefdb **out, git_repository *repo)
{
	git_specialrefdb_backend *backend;
	git_specialrefdb *db;
	int error;

	GIT_ASSERT_ARG(out);
	GIT_ASSERT_ARG(repo);

	*out = NULL;

	if ((error = git_specialrefdb_new(&db, repo)) < 0)
		goto out;

	if ((error = git_specialrefdb_git(&backend, repo)) < 0)
		goto out;

	db->repo = repo;
	db->backend = backend;

	*out = db;

out:
	if (error)
		git_specialrefdb_free(db);

	return error;
}

int git_specialrefdb_lookup_head(git_reference **out, git_specialrefdb *db)
{
	return db->backend->lookup_head(out, db->backend);
}

void git_specialrefdb__free(git_specialrefdb *db)
{
	if (db->backend->free)
		db->backend->free(db->backend);

	git__memzero(db, sizeof(*db));
	git__free(db);
}

void git_specialrefdb_free(git_specialrefdb *db)
{
	if (db == NULL)
		return;

	GIT_REFCOUNT_DEC(db, git_specialrefdb__free);
}
