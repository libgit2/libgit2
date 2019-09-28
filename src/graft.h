/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_graft_h__
#define INCLUDE_graft_h__

#include "common.h"
#include "oidarray.h"
#include "oidmap.h"

/** graft commit */
typedef struct {
	git_oid oid;
	git_array_oid_t parents;
} git_commit_graft;

/* A special type of git_oidmap with git_commit_grafts as values */
typedef git_oidmap git_graftmap;

int git__graft_register(git_graftmap *grafts, const git_oid *oid, git_array_oid_t parents);
int git__graft_unregister(git_graftmap *grafts, const git_oid *oid);
void git__graft_clear(git_graftmap *grafts);

int git__graft_for_oid(git_commit_graft **out, git_graftmap *grafts, const git_oid *oid);

#endif
