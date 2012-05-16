/*
 * Copyright (C) 2009-2012 the libgit2 contributors
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
};


GIT_INLINE(bool) git_tree_entry__is_tree(const struct git_tree_entry *e)
{
	return (S_ISDIR(e->attr) && !S_ISGITLINK(e->attr));
}

void git_tree__free(git_tree *tree);
int git_tree__parse(git_tree *tree, git_odb_object *obj);

/**
 * Lookup the first position in the tree with a given prefix.
 *
 * @param tree a previously loaded tree.
 * @param prefix the beginning of a path to find in the tree.
 * @return index of the first item at or after the given prefix.
 */
int git_tree__prefix_position(git_tree *tree, const char *prefix);


#endif
