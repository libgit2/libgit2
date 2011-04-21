#ifndef INCLUDE_index_h__
#define INCLUDE_index_h__

#include "fileops.h"
#include "filebuf.h"
#include "vector.h"
#include "git2/odb.h"
#include "git2/index.h"

struct git_index_tree {
	char *name;

	struct git_index_tree *parent;
	struct git_index_tree **children;
	size_t children_count;

	size_t entries;
	git_oid oid;
};

typedef struct git_index_tree git_index_tree;

struct git_index {
	git_repository *repository;
	char *index_file_path;

	time_t last_modified;
	git_vector entries;

	unsigned int on_disk:1;
	git_index_tree *tree;

	git_vector unmerged;
};

#endif
