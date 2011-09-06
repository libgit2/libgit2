/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_index_h__
#define INCLUDE_index_h__

#include "fileops.h"
#include "filebuf.h"
#include "vector.h"
#include "tree-cache.h"
#include "git2/odb.h"
#include "git2/index.h"

#define GIT_INDEX_FILE "index"
#define GIT_INDEX_FILE_MODE 0666

struct git_index {
	git_repository *repository;
	char *index_file_path;

	time_t last_modified;
	git_vector entries;

	unsigned int on_disk:1;
	git_tree_cache *tree;

	git_vector unmerged;
};

#endif
