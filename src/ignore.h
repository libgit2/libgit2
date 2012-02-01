/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_ignore_h__
#define INCLUDE_ignore_h__

#include "repository.h"
#include "vector.h"

typedef struct {
	git_repository *repo;
	char *dir;
	git_vector stack;
} git_ignores;

extern int git_ignore__for_path(git_repository *repo, const char *path, git_ignores *stack);
extern void git_ignore__free(git_ignores *stack);
extern int git_ignore__lookup(git_ignores *stack, const char *path, int *ignored);

#endif
