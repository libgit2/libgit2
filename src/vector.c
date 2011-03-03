/*
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2,
 * as published by the Free Software Foundation.
 *
 * In addition to the permissions in the GNU General Public License,
 * the authors give you unlimited permission to link the compiled
 * version of this file into combinations with other programs,
 * and to distribute those combinations without any restriction
 * coming from the use of this file.  (The General Public License
 * restrictions do apply in other respects; for example, they cover
 * modification of the file, and distribution when not linked into
 * a combined executable.)
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
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

	v->contents = realloc(v->contents, v->_alloc_size * sizeof(void *));
	if (v->contents == NULL)
		return GIT_ENOMEM;

	return GIT_SUCCESS;
}


void git_vector_free(git_vector *v)
{
	assert(v);
	free(v->contents);
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

	qsort(v->contents, v->length, sizeof(void *), v->_cmp);
	v->sorted = 1;
}

int git_vector_bsearch2(git_vector *v, git_vector_cmp key_lookup, const void *key)
{
	void **find;

	assert(v && key && key_lookup);

	/* need comparison function to sort the vector */
	if (v->_cmp == NULL)
		return GIT_ENOTFOUND;

	git_vector_sort(v);

	find = bsearch(key, v->contents, v->length, sizeof(void *), key_lookup);
	if (find != NULL)
		return (int)(find - v->contents);

	return GIT_ENOTFOUND;
}

int git_vector_search2(git_vector *v, git_vector_cmp key_lookup, const void *key)
{
	unsigned int i;

	assert(v && key && key_lookup);

	for (i = 0; i < v->length; ++i) {
		if (key_lookup(key, v->contents[i]) == 0)
			return i;
	}

	return GIT_ENOTFOUND;
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
	if (v->_cmp == NULL)
		return GIT_ENOTFOUND;

	return git_vector_bsearch2(v, v->_cmp, key);
}

int git_vector_remove(git_vector *v, unsigned int idx)
{
	unsigned int i;

	assert(v);

	if (idx >= v->length || v->length == 0)
		return GIT_ENOTFOUND;

	for (i = idx; i < v->length - 1; ++i)
		v->contents[i] = v->contents[i + 1];

	v->length--;
	return GIT_SUCCESS;
}

void git_vector_clear(git_vector *v)
{
	assert(v);
	v->length = 0;
	v->sorted = 1;
}


