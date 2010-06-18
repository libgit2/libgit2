#ifndef INCLUDE_tree_h__
#define INCLUDE_tree_h__

#include <git/tree.h>
#include "revobject.h"

struct git_tree {
	git_revpool_object object;
};

void git_tree__free(git_tree *tree);

#endif
