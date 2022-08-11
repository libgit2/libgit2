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

typedef struct git_grafts git_grafts;

extern bool git_shallow__enabled;

int git_grafts_new(git_grafts **out);
int git_grafts_from_file(git_grafts **out, const char *path);
void git_grafts_free(git_grafts *grafts);
void git_grafts_clear(git_grafts *grafts);

int git_grafts_refresh(git_grafts *grafts);
int git_grafts_parse(git_grafts *grafts, const char *content, size_t contentlen);
int git_grafts_add(git_grafts *grafts, const git_oid *oid, git_array_oid_t parents);
int git_grafts_remove(git_grafts *grafts, const git_oid *oid);
int git_grafts_get(git_commit_graft **out, git_grafts *grafts, const git_oid *oid);
int git_grafts_get_oids(git_array_oid_t *out, git_grafts *grafts);
size_t git_grafts_size(git_grafts *grafts);

#endif
