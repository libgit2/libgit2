#ifndef INCLUDE_vector_h__
#define INCLUDE_vector_h__

#include "git2/common.h"


typedef int (*git_vector_cmp)(const void *, const void *);
typedef int (*git_vector_srch)(const void *, const void *);

typedef struct git_vector {
	unsigned int _alloc_size;
	git_vector_cmp _cmp;
	git_vector_srch _srch;

	void **contents;
	unsigned int length;
} git_vector;


int git_vector_init(git_vector *v, unsigned int initial_size, git_vector_cmp cmp, git_vector_srch srch);
void git_vector_free(git_vector *v);
void git_vector_clear(git_vector *v);

int git_vector_search(git_vector *v, const void *key);
void git_vector_sort(git_vector *v);

GIT_INLINE(void *) git_vector_get(git_vector *v, unsigned int position)
{
	return (position < v->length) ? v->contents[position] : NULL;
}

int git_vector_insert(git_vector *v, void *element);
int git_vector_remove(git_vector *v, unsigned int idx);

#endif
