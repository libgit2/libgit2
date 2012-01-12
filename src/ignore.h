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

extern int git_ignore__for_path(git_repository *repo, const char *path, git_vector *stack);
extern void git_ignore__free(git_vector *stack);
extern int git_ignore__lookup(git_vector *stack, const char *path, int *ignored);

extern int git_ignore_is_ignored(git_repository *repo, const char *path, int *ignored);

#endif
