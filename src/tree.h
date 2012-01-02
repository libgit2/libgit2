/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_tree_h__
#define INCLUDE_tree_h__

#include "git2/tree.h"
#include "repository.h"
#include "odb.h"
#include "vector.h"

struct git_tree_entry {
	unsigned int attr;
	char *filename;
	git_oid oid;
	size_t filename_len;
	int removed;
};

struct git_tree {
	git_object object;
	git_vector entries;
};

struct git_treebuilder {
	git_vector entries;
	size_t entry_count;
};


GIT_INLINE(unsigned int) entry_is_tree(const struct git_tree_entry *e)
{
	return e->attr & 040000;
}

void git_tree__free(git_tree *tree);
int git_tree__parse(git_tree *tree, git_odb_object *obj);

#endif
