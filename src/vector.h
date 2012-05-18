/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_vector_h__
#define INCLUDE_vector_h__

#include "git2/common.h"

typedef int (*git_vector_cmp)(const void *, const void *);

typedef struct git_vector {
	unsigned int _alloc_size;
	git_vector_cmp _cmp;
	void **contents;
	unsigned int length;
	int sorted;
} git_vector;

#define GIT_VECTOR_INIT {0}

int git_vector_init(git_vector *v, unsigned int initial_size, git_vector_cmp cmp);
void git_vector_free(git_vector *v);
void git_vector_clear(git_vector *v);
void git_vector_swap(git_vector *a, git_vector *b);

void git_vector_sort(git_vector *v);

int git_vector_search(git_vector *v, const void *entry);
int git_vector_search2(git_vector *v, git_vector_cmp cmp, const void *key);

int git_vector_bsearch3(
	unsigned int *at_pos, git_vector *v, git_vector_cmp cmp, const void *key);

GIT_INLINE(int) git_vector_bsearch(git_vector *v, const void *key)
{
	return git_vector_bsearch3(NULL, v, v->_cmp, key);
}

GIT_INLINE(int) git_vector_bsearch2(
	git_vector *v, git_vector_cmp cmp, const void *key)
{
	return git_vector_bsearch3(NULL, v, cmp, key);
}

GIT_INLINE(void *) git_vector_get(git_vector *v, unsigned int position)
{
	return (position < v->length) ? v->contents[position] : NULL;
}

GIT_INLINE(const void *) git_vector_get_const(const git_vector *v, unsigned int position)
{
	return (position < v->length) ? v->contents[position] : NULL;
}

#define GIT_VECTOR_GET(V,I) ((I) < (V)->length ? (V)->contents[(I)] : NULL)

GIT_INLINE(void *) git_vector_last(git_vector *v)
{
	return (v->length > 0) ? git_vector_get(v, v->length - 1) : NULL;
}

#define git_vector_foreach(v, iter, elem)	\
	for ((iter) = 0; (iter) < (v)->length && ((elem) = (v)->contents[(iter)], 1); (iter)++ )

#define git_vector_rforeach(v, iter, elem)	\
	for ((iter) = (v)->length; (iter) > 0 && ((elem) = (v)->contents[(iter)-1], 1); (iter)-- )

int git_vector_insert(git_vector *v, void *element);
int git_vector_insert_sorted(git_vector *v, void *element,
	int (*on_dup)(void **old, void *new));
int git_vector_remove(git_vector *v, unsigned int idx);
void git_vector_pop(git_vector *v);
void git_vector_uniq(git_vector *v);

#endif
