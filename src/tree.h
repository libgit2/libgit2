#ifndef INCLUDE_tree_h__
#define INCLUDE_tree_h__

#include "git2/tree.h"
#include "repository.h"
#include "vector.h"

struct git_tree_entry {
	unsigned int attr;
	char *filename;
	git_oid oid;

	git_tree *owner;
};

struct git_tree {
	git_object object;
	git_vector entries;
};

void git_tree__free(git_tree *tree);
git_tree *git_tree__new(void);
int git_tree__parse(git_tree *tree);
int git_tree__writeback(git_tree *tree, git_odb_source *src);

#endif
