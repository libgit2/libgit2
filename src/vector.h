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

int git_vector_init(git_vector *v, unsigned int initial_size, git_vector_cmp cmp);
void git_vector_free(git_vector *v);
void git_vector_clear(git_vector *v);

int git_vector_search(git_vector *v, const void *entry);
int git_vector_search2(git_vector *v, git_vector_cmp cmp, const void *key);

int git_vector_bsearch(git_vector *v, const void *entry);
int git_vector_bsearch2(git_vector *v, git_vector_cmp cmp, const void *key);

void git_vector_sort(git_vector *v);

GIT_INLINE(void *) git_vector_get(git_vector *v, unsigned int position)
{
	return (position < v->length) ? v->contents[position] : NULL;
}

#define git_vector_foreach(v, iter, elem)	\
	for ((iter) = 0; (iter) < (v)->length && ((elem) = (v)->contents[(iter)], 1); (iter)++ )

int git_vector_insert(git_vector *v, void *element);
int git_vector_remove(git_vector *v, unsigned int idx);
void git_vector_uniq(git_vector *v);
#endif
