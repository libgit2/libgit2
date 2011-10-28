/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "repository.h"
#include "vector.h"

static const double resize_factor = 1.75;
static const size_t minimum_size = 8;

static int resize_vector(git_vector *v)
{
	v->_alloc_size = ((unsigned int)(v->_alloc_size * resize_factor)) + 1;
	if (v->_alloc_size < minimum_size)
		v->_alloc_size = minimum_size;

	v->contents = git__realloc(v->contents, v->_alloc_size * sizeof(void *));
	if (v->contents == NULL)
		return GIT_ENOMEM;

	return GIT_SUCCESS;
}


void git_vector_free(git_vector *v)
{
	assert(v);
	git__free(v->contents);
}

int git_vector_init(git_vector *v, unsigned int initial_size, git_vector_cmp cmp)
{
	assert(v);

	memset(v, 0x0, sizeof(git_vector));

	if (initial_size == 0)
		initial_size = minimum_size;

	v->_alloc_size = initial_size;
	v->_cmp = cmp;

	v->length = 0;
	v->sorted = 1;

	v->contents = git__malloc(v->_alloc_size * sizeof(void *));
	if (v->contents == NULL)
		return GIT_ENOMEM;

	return GIT_SUCCESS;
}

int git_vector_insert(git_vector *v, void *element)
{
	assert(v);

	if (v->length >= v->_alloc_size) {
		if (resize_vector(v) < 0)
			return GIT_ENOMEM;
	}

	v->contents[v->length++] = element;
	v->sorted = 0;

	return GIT_SUCCESS;
}

void git_vector_sort(git_vector *v)
{
	assert(v);

	if (v->sorted || v->_cmp == NULL)
		return;

	git__tsort(v->contents, v->length, v->_cmp);
	v->sorted = 1;
}

int git_vector_bsearch2(git_vector *v, git_vector_cmp key_lookup, const void *key)
{
	void **find;

	assert(v && key && key_lookup);

	/* need comparison function to sort the vector */
	if (v->_cmp == NULL)
		return git__throw(GIT_ENOTFOUND, "Can't sort vector. No comparison function set");

	git_vector_sort(v);

	find = git__bsearch(key, v->contents, v->length, key_lookup);
	if (find != NULL)
		return (int)(find - v->contents);

	return git__throw(GIT_ENOTFOUND, "Can't find element");
}

int git_vector_search2(git_vector *v, git_vector_cmp key_lookup, const void *key)
{
	unsigned int i;

	assert(v && key && key_lookup);

	for (i = 0; i < v->length; ++i) {
		if (key_lookup(key, v->contents[i]) == 0)
			return i;
	}

	return git__throw(GIT_ENOTFOUND, "Can't find element");
}

static int strict_comparison(const void *a, const void *b)
{
	return (a == b) ? 0 : -1;
}

int git_vector_search(git_vector *v, const void *entry)
{
	return git_vector_search2(v, v->_cmp ? v->_cmp : strict_comparison, entry);
}

int git_vector_bsearch(git_vector *v, const void *key)
{
	return git_vector_bsearch2(v, v->_cmp, key);
}

int git_vector_remove(git_vector *v, unsigned int idx)
{
	unsigned int i;

	assert(v);

	if (idx >= v->length || v->length == 0)
		return git__throw(GIT_ENOTFOUND, "Can't remove element. Index out of bounds");

	for (i = idx; i < v->length - 1; ++i)
		v->contents[i] = v->contents[i + 1];

	v->length--;
	return GIT_SUCCESS;
}

void git_vector_uniq(git_vector *v)
{
	git_vector_cmp cmp;
	unsigned int i, j;

	if (v->length <= 1)
		return;

	git_vector_sort(v);
	cmp = v->_cmp ? v->_cmp : strict_comparison;

	for (i = 0, j = 1 ; j < v->length; ++j)
		if (!cmp(v->contents[i], v->contents[j]))
			v->contents[i] = v->contents[j];
		else
			v->contents[++i] = v->contents[j];

	v->length -= j - i - 1;
}

void git_vector_clear(git_vector *v)
{
	assert(v);
	v->length = 0;
	v->sorted = 1;
}


