/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 *
 * This file is based on a modified version of the priority queue found
 * in the Apache project and libpqueue library.
 * 
 * https://github.com/vy/libpqueue
 * 
 * Original file notice:
 * 
 * Copyright 2010 Volkan Yazici <volkan.yazici@gmail.com>
 * Copyright 2006-2010 The Apache Software Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy of
 * the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations under
 * the License.
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
	q->d = git__malloc((n + 1) * sizeof(void *));
	GITERR_CHECK_ALLOC(q->d);

	q->size = 1;
	q->avail = q->step = (n + 1); /* see comment above about n+1 */
	q->cmppri = cmppri;

	return 0;
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
		tmp = git__realloc(q->d, sizeof(void *) * newsize);
		GITERR_CHECK_ALLOC(tmp);

		q->d = tmp;
		q->avail = newsize;
	}

	/* insert item */
	i = q->size++;
	q->d[i] = d;
	bubble_up(q, i);

	return 0;
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
