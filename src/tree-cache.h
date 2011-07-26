/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#ifndef INCLUDE_tree_cache_h__
#define INCLUDE_tree_cache_h__

#include "common.h"
#include "git2/oid.h"

struct git_tree_cache {
	char *name;

	struct git_tree_cache *parent;
	struct git_tree_cache **children;
	size_t children_count;

	ssize_t entries;
	git_oid oid;
};

typedef struct git_tree_cache git_tree_cache;

int git_tree_cache_read(git_tree_cache **tree, const char *buffer, size_t buffer_size);
void git_tree_cache_free(git_tree_cache *tree);

#endif
