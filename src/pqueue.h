/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#ifndef INCLUDE_pqueue_h__
#define INCLUDE_pqueue_h__

/** callback functions to get/set/compare the priority of an element */
typedef int (*git_pqueue_cmp)(void *a, void *b);

/** the priority queue handle */
typedef struct {
	size_t size, avail, step;
	git_pqueue_cmp cmppri;
	void **d;
} git_pqueue;


/**
 * initialize the queue
 *
 * @param n the initial estimate of the number of queue items for which memory
 *			should be preallocated
 * @param cmppri the callback function to compare two nodes of the queue
 *
 * @Return the handle or NULL for insufficent memory
 */
int git_pqueue_init(git_pqueue *q, size_t n, git_pqueue_cmp cmppri);


/**
 * free all memory used by the queue
 * @param q the queue
 */
void git_pqueue_free(git_pqueue *q);

/**
 * clear all the elements in the queue
 * @param q the queue
 */
void git_pqueue_clear(git_pqueue *q);

/**
 * return the size of the queue.
 * @param q the queue
 */
size_t git_pqueue_size(git_pqueue *q);


/**
 * insert an item into the queue.
 * @param q the queue
 * @param d the item
 * @return 0 on success
 */
int git_pqueue_insert(git_pqueue *q, void *d);


/**
 * pop the highest-ranking item from the queue.
 * @param p the queue
 * @param d where to copy the entry to
 * @return NULL on error, otherwise the entry
 */
void *git_pqueue_pop(git_pqueue *q);


/**
 * access highest-ranking item without removing it.
 * @param q the queue
 * @param d the entry
 * @return NULL on error, otherwise the entry
 */
void *git_pqueue_peek(git_pqueue *q);

#endif /* PQUEUE_H */
/** @} */

