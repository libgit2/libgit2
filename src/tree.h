#ifndef INCLUDE_tree_h__
#define INCLUDE_tree_h__

#include <git/tree.h>
#include "repository.h"

struct git_tree_entry {
	unsigned int attr;
	char *filename;
	git_oid oid;

	git_tree *owner;
};

struct git_tree {
	git_object object;

	git_tree_entry **entries;
	size_t entry_count;
	size_t array_size;
};

void git_tree__free(git_tree *tree);
int git_tree__parse(git_tree *tree);
int git_tree__writeback(git_tree *tree, git_odb_source *src);

#endif
