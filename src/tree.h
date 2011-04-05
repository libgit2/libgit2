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


void git_tree__free(git_tree *tree);
int git_tree__parse(git_tree *tree, git_odb_object *obj);

#endif
