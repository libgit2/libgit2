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
	size_t init_size,
	git_vector_cmp cmp)
{
	int error = git_vector_init(pq, init_size, cmp);

	if (!error) {
		/* mix in our flags */
		pq->flags |= flags;

		/* if fixed size heap, pretend vector is exactly init_size elements */
		if ((flags & GIT_PQUEUE_FIXED_SIZE) && init_size > 0)
			pq->_alloc_size = init_size;
	}

	return error;
}

static void pqueue_up(git_pqueue *pq, size_t el)
{
	size_t parent_el = PQUEUE_PARENT_OF(el);

	while (el > 0 && git_vector_cmp_elements(pq, parent_el, el) > 0) {
		git_vector_swap_elements(pq, el, parent_el);

		el = parent_el;
		parent_el = PQUEUE_PARENT_OF(el);
	}
}

static void pqueue_down(git_pqueue *pq, size_t el)
{
	size_t last = git_pqueue_size(pq);

	while (1) {
		size_t kid = PQUEUE_LCHILD_OF(el), rkid = PQUEUE_RCHILD_OF(el);
		if (kid >= last)
			break;
		if (rkid < last && git_vector_cmp_elements(pq, kid, rkid) > 0)
			kid = rkid;

		if (git_vector_cmp_elements(pq, el, kid) < 0)
			break;

		git_vector_swap_elements(pq, el, kid);
		el = kid;
	}
}

int git_pqueue_insert(git_pqueue *pq, void *item)
{
	int error = 0;

	/* if heap is full, pop the top element if new one should replace it */
	if ((pq->flags & GIT_PQUEUE_FIXED_SIZE) != 0 &&
		pq->length >= pq->_alloc_size)
	{
		/* skip this item if below min item in heap */
		if (pq->_cmp(item, git_vector_get(pq, 0)) <= 0)
			return 0;
		/* otherwise remove the min item before inserting new */
		(void)git_pqueue_pop(pq);
	}

	if (!(error = git_vector_insert(pq, item)))
		pqueue_up(pq, pq->length - 1);

	return error;
}

void *git_pqueue_pop(git_pqueue *pq)
{
	void *rval = git_pqueue_get(pq, 0);

	if (git_pqueue_size(pq) > 1) {
		/* move last item to top of heap, shrink, and push item down */
		pq->contents[0] = git_vector_last(pq);
		git_vector_pop(pq);
		pqueue_down(pq, 0);
	} else {
		/* all we need to do is shrink the heap in this case */
		git_vector_pop(pq);
	}

	return rval;
}
