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
	/** ignore case for entry sort order */
	GIT_ITERATOR_IGNORE_CASE = (1 << 0),
	/** force case sensitivity for entry sort order */
	GIT_ITERATOR_DONT_IGNORE_CASE = (1 << 1),
} git_iterator_flag_t;

typedef struct {
	int (*current)(const git_index_entry **, git_iterator *);
	int (*advance)(const git_index_entry **, git_iterator *);
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

/* Spool all iterator values, resort with alternative ignore_case value
 * and replace callbacks with spoolandsort alternates.
 */
extern int git_iterator_spoolandsort_push(git_iterator *iter, bool ignore_case);

/* Restore original callbacks - not required in most circumstances */
extern void git_iterator_spoolandsort_pop(git_iterator *iter);

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
GIT_INLINE(int) git_iterator_current(
	const git_index_entry **entry, git_iterator *iter)
{
	return iter->cb->current(entry, iter);
}

GIT_INLINE(int) git_iterator_advance(
	const git_index_entry **entry, git_iterator *iter)
{
	return iter->cb->advance(entry, iter);
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

GIT_INLINE(int) git_iterator_at_end(git_iterator *iter)
{
	return iter->cb->at_end(iter);
}

GIT_INLINE(git_iterator_type_t) git_iterator_type(git_iterator *iter)
{
	return iter->type;
}

GIT_INLINE(git_repository *) git_iterator_owner(git_iterator *iter)
{
	return iter->repo;
}

GIT_INLINE(git_iterator_flag_t) git_iterator_flags(git_iterator *iter)
{
	return iter->flags;
}

GIT_INLINE(bool) git_iterator_ignore_case(git_iterator *iter)
{
	return ((iter->flags & GIT_ITERATOR_IGNORE_CASE) != 0);
}

extern int git_iterator_current_tree_entry(
	const git_tree_entry **tree_entry, git_iterator *iter);

extern int git_iterator_current_parent_tree(
	const git_tree **tree_ptr, git_iterator *iter, const char *parent_path);

extern bool git_iterator_current_is_ignored(git_iterator *iter);

/**
 * Iterate into a directory.
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
extern int git_iterator_advance_into(
	const git_index_entry **entry, git_iterator *iter);

extern int git_iterator_cmp(
	git_iterator *iter, const char *path_prefix);

/**
 * Get full path of the current item from a workdir iterator.  This will
 * return NULL for a non-workdir iterator.  The git_buf is still owned by
 * the iterator; this is exposed just for efficiency.
 */
extern int git_iterator_current_workdir_path(
	git_buf **path, git_iterator *iter);

/* Return index pointer if index iterator, else NULL */
extern git_index *git_iterator_get_index(git_iterator *iter);

extern git_iterator_type_t git_iterator_inner_type(git_iterator *iter);

#endif
