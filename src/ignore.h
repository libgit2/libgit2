/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_ignore_h__
#define INCLUDE_ignore_h__

#include "repository.h"
#include "vector.h"

/* The git_ignores structure maintains three sets of ignores:
 * - internal ignores
 * - per directory ignores
 * - global ignores (at lower priority than the others)
 * As you traverse from one directory to another, you can push and pop
 * directories onto git_ignores list efficiently.
 */
typedef struct {
	git_repository *repo;
	git_buf dir;
	git_attr_file *ign_internal;
	git_vector ign_path;
	git_vector ign_global;
	unsigned int ignore_case:1;
} git_ignores;

extern int git_ignore__for_path(git_repository *repo, const char *path, git_ignores *ign);

extern int git_ignore__push_dir(git_ignores *ign, const char *dir);

extern int git_ignore__pop_dir(git_ignores *ign);

extern void git_ignore__free(git_ignores *ign);

extern int git_ignore__lookup(git_ignores *ign, const char *path, int *ignored);

#endif
