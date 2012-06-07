/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_iterator_h__
#define INCLUDE_iterator_h__

#include "common.h"
#include "git2/index.h"

typedef struct git_iterator git_iterator;

typedef enum {
	GIT_ITERATOR_EMPTY = 0,
	GIT_ITERATOR_TREE = 1,
	GIT_ITERATOR_INDEX = 2,
	GIT_ITERATOR_WORKDIR = 3
} git_iterator_type_t;

struct git_iterator {
	git_iterator_type_t type;
	char *start;
	char *end;
	int (*current)(git_iterator *, const git_index_entry **);
	int (*at_end)(git_iterator *);
	int (*advance)(git_iterator *, const git_index_entry **);
	int (*seek)(git_iterator *, const char *prefix);
	int (*reset)(git_iterator *);
	void (*free)(git_iterator *);
};

extern int git_iterator_for_nothing(git_iterator **iter);

extern int git_iterator_for_tree_range(
	git_iterator **iter, git_repository *repo, git_tree *tree,
	const char *start, const char *end);

GIT_INLINE(int) git_iterator_for_tree(
	git_iterator **iter, git_repository *repo, git_tree *tree)
{
	return git_iterator_for_tree_range(iter, repo, tree, NULL, NULL);
}

extern int git_iterator_for_index_range(
	git_iterator **iter, git_repository *repo,
	const char *start, const char *end);

GIT_INLINE(int) git_iterator_for_index(
	git_iterator **iter, git_repository *repo)
{
	return git_iterator_for_index_range(iter, repo, NULL, NULL);
}

extern int git_iterator_for_workdir_range(
	git_iterator **iter, git_repository *repo,
	const char *start, const char *end);

GIT_INLINE(int) git_iterator_for_workdir(
	git_iterator **iter, git_repository *repo)
{
	return git_iterator_for_workdir_range(iter, repo, NULL, NULL);
}


/* Entry is not guaranteed to be fully populated.  For a tree iterator,
 * we will only populate the mode, oid and path, for example.  For a workdir
 * iterator, we will not populate the oid.
 *
 * You do not need to free the entry.  It is still "owned" by the iterator.
 * Once you call `git_iterator_advance`, then content of the old entry is
 * no longer guaranteed to be valid.
 */
GIT_INLINE(int) git_iterator_current(
	git_iterator *iter, const git_index_entry **entry)
{
	return iter->current(iter, entry);
}

GIT_INLINE(int) git_iterator_at_end(git_iterator *iter)
{
	return iter->at_end(iter);
}

GIT_INLINE(int) git_iterator_advance(
	git_iterator *iter, const git_index_entry **entry)
{
	return iter->advance(iter, entry);
}

GIT_INLINE(int) git_iterator_seek(
	git_iterator *iter, const char *prefix)
{
	return iter->seek(iter, prefix);
}

GIT_INLINE(int) git_iterator_reset(git_iterator *iter)
{
	return iter->reset(iter);
}

GIT_INLINE(void) git_iterator_free(git_iterator *iter)
{
	if (iter == NULL)
		return;

	iter->free(iter);

	git__free(iter->start);
	git__free(iter->end);

	memset(iter, 0, sizeof(*iter));

	git__free(iter);
}

GIT_INLINE(git_iterator_type_t) git_iterator_type(git_iterator *iter)
{
	return iter->type;
}

extern int git_iterator_current_tree_entry(
	git_iterator *iter, const git_tree_entry **tree_entry);

extern int git_iterator_current_is_ignored(git_iterator *iter);

/**
 * Iterate into a workdir directory.
 *
 * Workdir iterators do not automatically descend into directories (so that
 * when comparing two iterator entries you can detect a newly created
 * directory in the workdir).  As a result, you may get S_ISDIR items from
 * a workdir iterator.  If you wish to iterate over the contents of the
 * directories you encounter, then call this function when you encounter
 * a directory.
 *
 * If there are no files in the directory, this will end up acting like a
 * regular advance and will skip past the directory, so you should be
 * prepared for that case.
 *
 * On non-workdir iterators or if not pointing at a directory, this is a
 * no-op and will not advance the iterator.
 */
extern int git_iterator_advance_into_directory(
	git_iterator *iter, const git_index_entry **entry);

extern int git_iterator_cmp(
	git_iterator *iter, const char *path_prefix);

#endif
