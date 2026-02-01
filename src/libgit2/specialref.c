/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "specialref.h"

#include "common.h"
#include "refs.h"
#include "repository.h"
#include "specialrefdb.h"

#include "git2/types.h"
#include "git2/refs.h"

int git_specialref_lookup_head(git_reference **out, git_repository *repo)
{
	git_specialrefdb *db;
	int error;

	if ((error = git_repository_specialrefdb__weakptr(&db, repo)) < 0)
		return error;

	return git_specialrefdb_lookup_head(out, db);
}
