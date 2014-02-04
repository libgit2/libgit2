/*
 * Copyright (C) the libgit2 contributors.  All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_pqueue_h__
#define INCLUDE_pqueue_h__

#include "vector.h"

typedef struct {
	git_vector values;
	size_t     initial_size;
	uint32_t   flags;
} git_pqueue;

enum {
	GIT_PQUEUE_DEFAULT = 0,
	GIT_PQUEUE_FIXED_SIZE = (1 << 0), /* don't grow heap, keep highest */
};

/**
 * Initialize priority queue
 *
 * @param pq The priority queue struct to initialize
 * @param flags Flags (see above) to control queue behavior
 * @param est_size The estimated/initial queue size
 * @param cmp The entry priority comparison function
 * @return 0 on success, <0 on error
 */
extern int git_pqueue_init(
	git_pqueue *pq,
	uint32_t flags,
	size_t est_size,
	git_vector_cmp cmp);

/**
 * Free the queue memory
 */
extern void git_pqueue_free(git_pqueue *pq);

/**
 * Get the number of items in the queue
 */
GIT_INLINE(size_t) git_pqueue_size(const git_pqueue *pq)
{
	return git_vector_length(&pq->values);
}

/**
 * Get an item in the queue
 */
GIT_INLINE(void *) git_pqueue_get(const git_pqueue *pq, size_t pos)
{
	return git_vector_get(&pq->values, pos);
}

/**
 * Insert a new item into the queue
 *
 * @param pq The priority queue
 * @param item Pointer to the item data
 * @return 0 on success, <0 on failure
 */
extern int git_pqueue_insert(git_pqueue *pq, void *item);

/**
 * Remove the top item in the priority queue
 *
 * @param pq The priority queue
 * @return item from heap on success, NULL if queue is empty
 */
extern void *git_pqueue_pop(git_pqueue *pq);

#endif
