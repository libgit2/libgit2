/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_index_h__
#define INCLUDE_index_h__

#include <stddef.h>

#include "fileops.h"
#include "filebuf.h"
#include "vector.h"
#include "tree-cache.h"
#include "git2/odb.h"
#include "git2/index.h"

#define GIT_INDEX_FILE "index"
#define GIT_INDEX_FILE_MODE 0666

#define INDEX_HEADER_SIG 0x44495243

#define INDEX_VERSION_NUMBER 2
#define INDEX_VERSION_NUMBER_EXT 3

struct git_index_operations {
	int (*parse)(git_index *index, const char *buffer, size_t buffer_size);
	int (*write)(git_index *index, git_filebuf *file);
};

extern struct git_index_operations git_index_v2_ops;

struct index_entry_time {
	uint32_t seconds;
	uint32_t nanoseconds;
};

struct git_index {
	git_refcount rc;

	unsigned int version;

	char *index_file_path;

	time_t last_modified;
	git_vector entries;

	unsigned int on_disk:1;

	unsigned int ignore_case:1;
	unsigned int distrust_filemode:1;
	unsigned int no_symlinks:1;

	git_tree_cache *tree;

	git_vector unmerged;

	struct git_index_operations *operations;
};

extern void git_index__init_entry_from_stat(struct stat *st, git_index_entry *entry);

extern unsigned int git_index__prefix_position(git_index *index, const char *path);

int index_error_invalid(const char *message);

#endif
