/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_attr_h__
#define INCLUDE_attr_h__

#include "attr_file.h"

typedef struct {
	int initialized;
	git_hashtable *files;	/* hash path to git_attr_file of rules */
	git_hashtable *macros;	/* hash name to vector<git_attr_assignment> */
} git_attr_cache;

extern int git_attr_cache__init(git_repository *repo);

extern int git_attr_cache__insert_macro(
	git_repository *repo, git_attr_rule *macro);

extern int git_attr_cache__push_file(
	git_repository *repo,
	git_vector     *stack,
	const char     *base,
	const char     *filename,
	int (*loader)(git_repository *, const char *, git_attr_file *));

/* returns GIT_SUCCESS if path is in cache */
extern int git_attr_cache__is_cached(git_repository *repo, const char *path);

#endif
