/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "pqueue.h"

#define left(i)	((i) << 1)
#define right(i) (((i) << 1) + 1)
#define parent(i) ((i) >> 1)

int git_pqueue_init(git_pqueue *q, size_t n, git_pqueue_cmp cmppri)
{
	assert(q);

	/* Need to allocate n+1 elements since element 0 isn't used. */
	if ((q->d = git__malloc((n + 1) * sizeof(void *))) == NULL)
		return GIT_ENOMEM;

	q->size = 1;
	q->avail = q->step = (n + 1); /* see comment above about n+1 */
	q->cmppri = cmppri;

	return GIT_SUCCESS;
}


void git_pqueue_free(git_pqueue *q)
{
	git__free(q->d);
	q->d = NULL;
}

void git_pqueue_clear(git_pqueue *q)
{
	q->size = 1;
}

size_t git_pqueue_size(git_pqueue *q)
{
	/* queue element 0 exists but doesn't count since it isn't used. */
	return (q->size - 1);
}


static void bubble_up(git_pqueue *q, size_t i)
{
	size_t parent_node;
	void *moving_node = q->d[i];

	for (parent_node = parent(i);
			((i > 1) && q->cmppri(q->d[parent_node], moving_node));
			i = parent_node, parent_node = parent(i)) {
		q->d[i] = q->d[parent_node];
	}

	q->d[i] = moving_node;
}


static size_t maxchild(git_pqueue *q, size_t i)
{
	size_t child_node = left(i);

	if (child_node >= q->size)
		return 0;

	if ((child_node + 1) < q->size &&
		q->cmppri(q->d[child_node], q->d[child_node + 1]))
		child_node++; /* use right child instead of left */

	return child_node;
}


static void percolate_down(git_pqueue *q, size_t i)
{
	size_t child_node;
	void *moving_node = q->d[i];

	while ((child_node = maxchild(q, i)) != 0 &&
			q->cmppri(moving_node, q->d[child_node])) {
		q->d[i] = q->d[child_node];
		i = child_node;
	}

	q->d[i] = moving_node;
}


int git_pqueue_insert(git_pqueue *q, void *d)
{
	void *tmp;
	size_t i;
	size_t newsize;

	if (!q) return 1;

	/* allocate more memory if necessary */
	if (q->size >= q->avail) {
		newsize = q->size + q->step;
		if ((tmp = git__realloc(q->d, sizeof(void *) * newsize)) == NULL)
			return GIT_ENOMEM;

		q->d = tmp;
		q->avail = newsize;
	}

	/* insert item */
	i = q->size++;
	q->d[i] = d;
	bubble_up(q, i);

	return GIT_SUCCESS;
}


void *git_pqueue_pop(git_pqueue *q)
{
	void *head;

	if (!q || q->size == 1)
		return NULL;

	head = q->d[1];
	q->d[1] = q->d[--q->size];
	percolate_down(q, 1);

	return head;
}


void *git_pqueue_peek(git_pqueue *q)
{
	if (!q || q->size == 1)
		return NULL;
	return q->d[1];
}
