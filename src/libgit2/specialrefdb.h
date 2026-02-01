/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_specialrefdb_h__
#define INCLUDE_specialrefdb_h__

#include "common.h"

#include "git2/specialrefdb.h"
#include "repository.h"

struct git_specialrefdb {
	git_refcount rc;
	git_repository *repo;
	git_specialrefdb_backend *backend;
};

int git_specialrefdb_lookup_head(git_reference **out, git_specialrefdb *db);
void git_specialrefdb_free(git_specialrefdb *db);

#endif
