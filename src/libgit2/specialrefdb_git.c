/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "specialref.h"

#include "git2/sys/specialrefdb_backend.h"

typedef struct specialrefdb_git_backend {
	git_specialrefdb_backend parent;

	git_repository *repo;
} specialrefdb_git_backend;

static int specialrefdb_git_backend__lookup_head(git_reference **out, git_specialrefdb_backend *b)
{
	specialrefdb_git_backend *backend = (specialrefdb_git_backend *)b;

	GIT_ASSERT(backend);

	return git_reference_lookup(out, backend->repo, "HEAD");
}

static void specialrefdb_git_backend__free(git_specialrefdb_backend *b)
{
	specialrefdb_git_backend *backend = (specialrefdb_git_backend *)b;

	if (!b)
		return;

	git__free(backend);
}

int git_specialrefdb_git(
	git_specialrefdb_backend **backend_out,
	git_repository *repo)
{
	specialrefdb_git_backend *backend;

	backend = git__calloc(1, sizeof(specialrefdb_git_backend));
	GIT_ERROR_CHECK_ALLOC(backend);

	backend->repo = repo;

	backend->parent.lookup_head = &specialrefdb_git_backend__lookup_head;
	backend->parent.free = &specialrefdb_git_backend__free;

	*backend_out = (git_specialrefdb_backend *)backend;
	return 0;
}
