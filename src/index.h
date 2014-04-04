/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
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
	git_refcount rc;

	char *index_file_path;

	git_futils_filestamp stamp;
	git_vector entries;

	unsigned int on_disk:1;

	unsigned int ignore_case:1;
	unsigned int distrust_filemode:1;
	unsigned int no_symlinks:1;

	git_tree_cache *tree;

	git_vector names;
	git_vector reuc;

	git_vector_cmp entries_cmp_path;
	git_vector_cmp entries_search;
	git_vector_cmp entries_search_path;
	git_vector_cmp reuc_search;
};

struct git_index_conflict_iterator {
	git_index *index;
	size_t cur;
};

extern void git_index_entry__init_from_stat(
	git_index_entry *entry, struct stat *st, bool trust_mode);

extern size_t git_index__prefix_position(git_index *index, const char *path);

extern int git_index_entry__cmp(const void *a, const void *b);
extern int git_index_entry__cmp_icase(const void *a, const void *b);

extern int git_index__find(
	size_t *at_pos, git_index *index, const char *path, size_t path_len, int stage);

extern void git_index__set_ignore_case(git_index *index, bool ignore_case);

extern unsigned int git_index__create_mode(unsigned int mode);

GIT_INLINE(const git_futils_filestamp *) git_index__filestamp(git_index *index)
{
   return &index->stamp;
}

extern int git_index__changed_relative_to(git_index *index, const git_futils_filestamp *fs);

#endif
