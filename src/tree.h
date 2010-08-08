#ifndef INCLUDE_tree_h__
#define INCLUDE_tree_h__

#include <git/tree.h>
#include "repository.h"

struct git_tree_entry {

	unsigned int attr;
	char *filename;
	git_oid oid;

	struct git_tree_entry *next;
};

typedef struct git_tree_entry git_tree_entry;

struct git_tree {
	git_repository_object object;

	size_t byte_size;
	git_tree_entry *entries;
	unsigned int entry_count;
};

void git_tree__free(git_tree *tree);
int git_tree__parse(git_tree *tree);

#endif
