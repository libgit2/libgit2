/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
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

typedef struct git_iterator git_iterator;

typedef enum {
	GIT_ITERATOR_TYPE_EMPTY = 0,
	GIT_ITERATOR_TYPE_TREE = 1,
	GIT_ITERATOR_TYPE_INDEX = 2,
	GIT_ITERATOR_TYPE_WORKDIR = 3,
	GIT_ITERATOR_TYPE_SPOOLANDSORT = 4
} git_iterator_type_t;

typedef enum {
	/** Ignore case for entry sort order */
	GIT_ITERATOR_IGNORE_CASE      = (1 << 0),
	/** Force case sensitivity in entry sort order */
	GIT_ITERATOR_DONT_IGNORE_CASE = (1 << 1),
	/** Return tree items in addition to blob items */
	GIT_ITERATOR_INCLUDE_TREES    = (1 << 2),
	/** Don't flatten trees, requiring advance_into (implies INCLUDE_TREES) */
	GIT_ITERATOR_DONT_AUTOEXPAND  = (1 << 3),
} git_iterator_flag_t;

typedef struct {
	int (*current)(const git_index_entry **, git_iterator *);
	int (*advance)(const git_index_entry **, git_iterator *);
	int (*advance_into)(const git_index_entry **, git_iterator *);
	int (*seek)(git_iterator *, const char *prefix);
	int (*reset)(git_iterator *, const char *start, const char *end);
	int (*at_end)(git_iterator *);
	void (*free)(git_iterator *);
} git_iterator_callbacks;

struct git_iterator {
	git_iterator_type_t type;
	git_iterator_callbacks *cb;
	git_repository *repo;
	char *start;
	char *end;
	int (*prefixcomp)(const char *str, const char *prefix);
	unsigned int flags;
};

extern int git_iterator_for_nothing(
	git_iterator **out,
	git_iterator_flag_t flags,
	const char *start,
	const char *end);

/* tree iterators will match the ignore_case value from the index of the
 * repository, unless you override with a non-zero flag value
 */
extern int git_iterator_for_tree(
	git_iterator **out,
	git_tree *tree,
	git_iterator_flag_t flags,
	const char *start,
	const char *end);

/* index iterators will take the ignore_case value from the index; the
 * ignore_case flags are not used
 */
extern int git_iterator_for_index(
	git_iterator **out,
	git_index *index,
	git_iterator_flag_t flags,
	const char *start,
	const char *end);

/* workdir iterators will match the ignore_case value from the index of the
 * repository, unless you override with a non-zero flag value
 */
extern int git_iterator_for_workdir(
	git_iterator **out,
	git_repository *repo,
	git_iterator_flag_t flags,
	const char *start,
	const char *end);

extern void git_iterator_free(git_iterator *iter);

/* Return a git_index_entry structure for the current value the iterator
 * is looking at or NULL if the iterator is at the end.
 *
 * The entry may noy be fully populated.  Tree iterators will only have a
 * value mode, OID, and path.  Workdir iterators will not have an OID (but
 * you can use `git_iterator_current_oid()` to calculate it on demand).
 *
 * You do not need to free the entry.  It is still "owned" by the iterator.
 * Once you call `git_iterator_advance()` then the old entry is no longer
 * guaranteed to be valid - it may be freed or just overwritten in place.
 */
GIT_INLINE(int) git_iterator_current(const git_index_entry **e, git_iterator *i)
{
	return i->cb->current(e, i);
}

GIT_INLINE(int) git_iterator_at_end(git_iterator *i)
{
	return i->cb->at_end(i);
}

GIT_INLINE(int) git_iterator_advance(const git_index_entry **e, git_iterator *i)
{
	return i->cb->advance(e, i);
}

/**
 * Iterate into a directory when GIT_ITERATOR_DONT_AUTOEXPAND is set.
 *
 * Normally, git_iterator_advance() will allow you to step through all
 * the items being iterated over (either including or excluding trees,
 * depending on use of GIT_ITERATOR_INCLUDE_TREES).
 *
 * When you use the GIT_ITERATOR_DONT_AUTOEXPAND flag, however, advance will
 * skip to the next sibling entry when you are pointing at a tree instead of
 * going to the first child of the tree.  Use this function to advance to
 * the first child of the tree.
 *
 * If the current item is not a tree, this is a no-op.
 *
 * For working directory iterators, if the tree (i.e. directory) is empty,
 * this returns GIT_ENOTFOUND and does not advance.  For tree and index
 * iterators, that cannot happen since empty trees are not stored by git.
 */
GIT_INLINE(int) git_iterator_advance_into(
	const git_index_entry **e, git_iterator *i)
{
	return i->cb->advance_into(e, i);
}

GIT_INLINE(int) git_iterator_seek(git_iterator *i, const char *pfx)
{
	return i->cb->seek(i, pfx);
}

GIT_INLINE(int) git_iterator_reset(
	git_iterator *i, const char *start, const char *end)
{
	return i->cb->reset(i, start, end);
}

GIT_INLINE(git_iterator_type_t) git_iterator_type(git_iterator *i)
{
	return i->type;
}

GIT_INLINE(git_repository *) git_iterator_owner(git_iterator *i)
{
	return i->repo;
}

GIT_INLINE(git_iterator_flag_t) git_iterator_flags(git_iterator *i)
{
	return i->flags;
}

GIT_INLINE(bool) git_iterator_ignore_case(git_iterator *i)
{
	return ((i->flags & GIT_ITERATOR_IGNORE_CASE) != 0);
}

extern int git_iterator_set_ignore_case(git_iterator *i, bool ignore_case);


extern int git_iterator_current_tree_entry(
	const git_tree_entry **entry_out, git_iterator *iter);

extern int git_iterator_current_parent_tree(
	const git_tree **tree_out, git_iterator *iter, const char *parent_path);

extern bool git_iterator_current_is_ignored(git_iterator *iter);

extern int git_iterator_current_oid(git_oid *oid_out, git_iterator *iter);

/**
 * Get full path of the current item from a workdir iterator.  This will
 * return NULL for a non-workdir iterator.  The git_buf is still owned by
 * the iterator; this is exposed just for efficiency.
 */
extern int git_iterator_current_workdir_path(
	git_buf **path, git_iterator *iter);


extern int git_iterator_cmp(
	git_iterator *iter, const char *path_prefix);

/* returns index pointer if index iterator, else NULL */
extern git_index *git_iterator_get_index(git_iterator *iter);

#endif
