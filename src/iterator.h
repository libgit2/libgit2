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
#include "vector.h"
#include "buffer.h"

#define ITERATOR_PREFIXCMP(ITER, STR, PREFIX)	(((ITER).ignore_case) ? \
	git__prefixcmp_icase((STR), (PREFIX)) : \
	git__prefixcmp((STR), (PREFIX)))

typedef struct git_iterator git_iterator;

typedef enum {
	GIT_ITERATOR_EMPTY = 0,
	GIT_ITERATOR_TREE = 1,
	GIT_ITERATOR_INDEX = 2,
	GIT_ITERATOR_WORKDIR = 3,
	GIT_ITERATOR_SPOOLANDSORT = 4
} git_iterator_type_t;

typedef struct {
	int (*current)(git_iterator *, const git_index_entry **);
	int (*at_end)(git_iterator *);
	int (*advance)(git_iterator *, const git_index_entry **);
	int (*seek)(git_iterator *, const char *prefix);
	int (*reset)(git_iterator *, const char *start, const char *end);
	void (*free)(git_iterator *);
} git_iterator_callbacks;

struct git_iterator {
	git_iterator_type_t type;
	git_iterator_callbacks *cb;
	git_repository *repo;
	char *start;
	char *end;
	bool ignore_case;
};

extern int git_iterator_for_nothing(git_iterator **iter);

extern int git_iterator_for_tree_range(
	git_iterator **iter, git_tree *tree,
	const char *start, const char *end);

GIT_INLINE(int) git_iterator_for_tree(
	git_iterator **iter, git_tree *tree)
{
	return git_iterator_for_tree_range(iter, tree, NULL, NULL);
}

extern int git_iterator_for_index_range(
	git_iterator **iter, git_index *index, const char *start, const char *end);

GIT_INLINE(int) git_iterator_for_index(
	git_iterator **iter, git_index *index)
{
	return git_iterator_for_index_range(iter, index, NULL, NULL);
}

extern int git_iterator_for_repo_index_range(
	git_iterator **iter, git_repository *repo,
	const char *start, const char *end);

GIT_INLINE(int) git_iterator_for_repo_index(
	git_iterator **iter, git_repository *repo)
{
	return git_iterator_for_repo_index_range(iter, repo, NULL, NULL);
}

extern int git_iterator_for_workdir_range(
	git_iterator **iter, git_repository *repo,
	const char *start, const char *end);

GIT_INLINE(int) git_iterator_for_workdir(
	git_iterator **iter, git_repository *repo)
{
	return git_iterator_for_workdir_range(iter, repo, NULL, NULL);
}

/* Spool all iterator values, resort with alternative ignore_case value
 * and replace callbacks with spoolandsort alternates.
 */
extern int git_iterator_spoolandsort_push(git_iterator *iter, bool ignore_case);

/* Restore original callbacks - not required in most circumstances */
extern void git_iterator_spoolandsort_pop(git_iterator *iter);

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
	return iter->cb->current(iter, entry);
}

GIT_INLINE(int) git_iterator_at_end(git_iterator *iter)
{
	return iter->cb->at_end(iter);
}

GIT_INLINE(int) git_iterator_advance(
	git_iterator *iter, const git_index_entry **entry)
{
	return iter->cb->advance(iter, entry);
}

GIT_INLINE(int) git_iterator_seek(
	git_iterator *iter, const char *prefix)
{
	return iter->cb->seek(iter, prefix);
}

GIT_INLINE(int) git_iterator_reset(
	git_iterator *iter, const char *start, const char *end)
{
	return iter->cb->reset(iter, start, end);
}

GIT_INLINE(void) git_iterator_free(git_iterator *iter)
{
	if (iter == NULL)
		return;

	iter->cb->free(iter);

	git__free(iter->start);
	git__free(iter->end);

	memset(iter, 0, sizeof(*iter));

	git__free(iter);
}

GIT_INLINE(git_iterator_type_t) git_iterator_type(git_iterator *iter)
{
	return iter->type;
}

GIT_INLINE(git_repository *) git_iterator_owner(git_iterator *iter)
{
	return iter->repo;
}

extern int git_iterator_current_tree_entry(
	git_iterator *iter, const git_tree_entry **tree_entry);

extern int git_iterator_current_parent_tree(
	git_iterator *iter, const char *parent_path, const git_tree **tree_ptr);

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

/**
 * Get the full path of the current item from a workdir iterator.
 * This will return NULL for a non-workdir iterator.
 */
extern int git_iterator_current_workdir_path(
	git_iterator *iter, git_buf **path);

#endif
