/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "pqueue.h"
#include "util.h"

#define PQUEUE_LCHILD_OF(I) (((I)<<1)+1)
#define PQUEUE_RCHILD_OF(I) (((I)<<1)+2)
#define PQUEUE_PARENT_OF(I) (((I)-1)>>1)

int git_pqueue_init(
	git_pqueue *pq,
	uint32_t flags,
	size_t est_size,
	git_vector_cmp cmp)
{
	pq->flags = flags;
	pq->initial_size = est_size;
	return git_vector_init(&pq->values, est_size, cmp);
}

void git_pqueue_free(git_pqueue *pq)
{
	git_vector_free(&pq->values);
}

static void pqueue_up(git_pqueue *pq, size_t el)
{
	size_t parent_el = PQUEUE_PARENT_OF(el);

	while (el > 0 && git_vector_cmp_elements(&pq->values, parent_el, el) > 0) {
		git_vector_swap_elements(&pq->values, el, parent_el);

		el = parent_el;
		parent_el = PQUEUE_PARENT_OF(el);
	}
}

static void pqueue_down(git_pqueue *pq, size_t el)
{
	size_t last = git_vector_length(&pq->values);

	while (1) {
		size_t kid = PQUEUE_LCHILD_OF(el), rkid = PQUEUE_RCHILD_OF(el);
		if (kid >= last)
			break;
		if (rkid < last && git_vector_cmp_elements(&pq->values, kid, rkid) > 0)
			kid = rkid;

		if (git_vector_cmp_elements(&pq->values, el, kid) < 0)
			break;

		git_vector_swap_elements(&pq->values, el, kid);
		el = kid;
	}
}

int git_pqueue_insert(git_pqueue *pq, void *item)
{
	int error = 0;

	/* if heap is full, pop the top element if new one should replace it */
	if ((pq->flags & GIT_PQUEUE_FIXED_SIZE) != 0 &&
		pq->values.length >= pq->initial_size)
	{
		/* skip item if below min item in heap */
		if (pq->values._cmp(item, git_vector_get(&pq->values, 0)) <= 0)
			return 0;
		(void)git_pqueue_pop(pq);
	}

	error = git_vector_insert(&pq->values, item);

	if (!error)
		pqueue_up(pq, pq->values.length - 1);

	return error;
}

void *git_pqueue_pop(git_pqueue *pq)
{
	void *rval = git_vector_get(&pq->values, 0);

	if (git_vector_length(&pq->values) > 1) {
		pq->values.contents[0] = git_vector_last(&pq->values);
		git_vector_pop(&pq->values);
		pqueue_down(pq, 0);
	} else {
		git_vector_pop(&pq->values);
	}

	return rval;
}
