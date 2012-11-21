/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "repository.h"
#include "vector.h"

static const double git_vector_resize_factor = 1.75;
static const size_t git_vector_minimum_size = 8;

static int resize_vector(git_vector *v)
{
	v->_alloc_size = (size_t)(v->_alloc_size * git_vector_resize_factor) + 1;
	if (v->_alloc_size < git_vector_minimum_size)
		v->_alloc_size = git_vector_minimum_size;

	v->contents = git__realloc(v->contents, v->_alloc_size * sizeof(void *));
	GITERR_CHECK_ALLOC(v->contents);

	return 0;
}

int git_vector_dup(git_vector *v, const git_vector *src, git_vector_cmp cmp)
{
	assert(v && src);

	v->_alloc_size = src->length;
	v->_cmp = cmp;
	v->length = src->length;
	v->sorted = src->sorted && cmp == src->_cmp;
	v->contents = git__malloc(src->length * sizeof(void *));
	GITERR_CHECK_ALLOC(v->contents);

	memcpy(v->contents, src->contents, src->length * sizeof(void *));

	return 0;
}

void git_vector_free(git_vector *v)
{
	assert(v);

	git__free(v->contents);
	v->contents = NULL;

	v->length = 0;
	v->_alloc_size = 0;
}

int git_vector_init(git_vector *v, size_t initial_size, git_vector_cmp cmp)
{
	assert(v);

	memset(v, 0x0, sizeof(git_vector));

	if (initial_size == 0)
		initial_size = git_vector_minimum_size;

	v->_alloc_size = initial_size;
	v->_cmp = cmp;

	v->length = 0;
	v->sorted = 1;

	v->contents = git__malloc(v->_alloc_size * sizeof(void *));
	GITERR_CHECK_ALLOC(v->contents);

	return 0;
}

int git_vector_insert(git_vector *v, void *element)
{
	assert(v);

	if (v->length >= v->_alloc_size &&
		resize_vector(v) < 0)
		return -1;

	v->contents[v->length++] = element;
	v->sorted = 0;

	return 0;
}

int git_vector_insert_sorted(
	git_vector *v, void *element, int (*on_dup)(void **old, void *new))
{
	int result;
	size_t pos;

	assert(v && v->_cmp);

	if (!v->sorted)
		git_vector_sort(v);

	if (v->length >= v->_alloc_size &&
		resize_vector(v) < 0)
		return -1;

	/* If we find the element and have a duplicate handler callback,
	 * invoke it.  If it returns non-zero, then cancel insert, otherwise
	 * proceed with normal insert.
	 */
	if (git__bsearch(v->contents, v->length, element, v->_cmp, &pos) >= 0 &&
		on_dup != NULL &&
		(result = on_dup(&v->contents[pos], element)) < 0)
		return result;

	/* shift elements to the right */
	if (pos < v->length) {
		memmove(v->contents + pos + 1, v->contents + pos,
		        (v->length - pos) * sizeof(void *));
	}

	v->contents[pos] = element;
	v->length++;
	return 0;
}

void git_vector_sort(git_vector *v)
{
	assert(v);

	if (v->sorted || v->_cmp == NULL)
		return;

	git__tsort(v->contents, v->length, v->_cmp);
	v->sorted = 1;
}

int git_vector_bsearch3(
	size_t *at_pos,
	git_vector *v,
	git_vector_cmp key_lookup,
	const void *key)
{
	int rval;
	size_t pos;

	assert(v && key && key_lookup);

	/* need comparison function to sort the vector */
	assert(v->_cmp != NULL);

	git_vector_sort(v);

	rval = git__bsearch(v->contents, v->length, key, key_lookup, &pos);

	if (at_pos != NULL)
		*at_pos = pos;

	return (rval >= 0) ? (int)pos : GIT_ENOTFOUND;
}

int git_vector_search2(
	const git_vector *v, git_vector_cmp key_lookup, const void *key)
{
	size_t i;

	assert(v && key && key_lookup);

	for (i = 0; i < v->length; ++i) {
		if (key_lookup(key, v->contents[i]) == 0)
			return (int)i;
	}

	return GIT_ENOTFOUND;
}

static int strict_comparison(const void *a, const void *b)
{
	return (a == b) ? 0 : -1;
}

int git_vector_search(const git_vector *v, const void *entry)
{
	return git_vector_search2(v, v->_cmp ? v->_cmp : strict_comparison, entry);
}

int git_vector_remove(git_vector *v, size_t idx)
{
	size_t i;

	assert(v);

	if (idx >= v->length || v->length == 0)
		return GIT_ENOTFOUND;

	for (i = idx; i < v->length - 1; ++i)
		v->contents[i] = v->contents[i + 1];

	v->length--;
	return 0;
}

void git_vector_pop(git_vector *v)
{
	if (v->length > 0)
		v->length--;
}

void git_vector_uniq(git_vector *v)
{
	git_vector_cmp cmp;
	size_t i, j;

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

void git_vector_remove_matching(
	git_vector *v, int (*match)(const git_vector *v, size_t idx))
{
	size_t i, j;

	for (i = 0, j = 0; j < v->length; ++j) {
		v->contents[i] = v->contents[j];

		if (!match(v, i))
			i++;
	}

	v->length = i;
}

void git_vector_clear(git_vector *v)
{
	assert(v);
	v->length = 0;
	v->sorted = 1;
}

void git_vector_swap(git_vector *a, git_vector *b)
{
	git_vector t;

	if (!a || !b || a == b)
		return;

	memcpy(&t, a, sizeof(t));
	memcpy(a, b, sizeof(t));
	memcpy(b, &t, sizeof(t));
}

int git_vector_resize_to(git_vector *v, size_t new_length)
{
	if (new_length <= v->length)
		return 0;

	while (new_length >= v->_alloc_size)
		if (resize_vector(v) < 0)
			return -1;

	memset(&v->contents[v->length], 0,
		sizeof(void *) * (new_length - v->length));

	v->length = new_length;

	return 0;
}

int git_vector_set(void **old, git_vector *v, size_t position, void *value)
{
	if (git_vector_resize_to(v, position + 1) < 0)
		return -1;

	if (old != NULL)
		*old = v->contents[position];

	v->contents[position] = value;

	return 0;
}
