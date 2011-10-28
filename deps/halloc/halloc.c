/*
 *	Copyright (c) 2004i-2010 Alex Pankratov. All rights reserved.
 *
 *	Hierarchical memory allocator, 1.2.1
 *	http://swapped.cc/halloc
 */

/*
 *	The program is distributed under terms of BSD license. 
 *	You can obtain the copy of the license by visiting:
 *	
 *	http://www.opensource.org/licenses/bsd-license.php
 */

#include <stdlib.h>  /* realloc */
#include <string.h>  /* memset & co */

#include "halloc.h"
#include "align.h"
#include "hlist.h"

/*
 *	block control header
 */
typedef struct hblock
{
#ifndef NDEBUG
#define HH_MAGIC    0x20040518L
	long          magic;
#endif
	hlist_item_t  siblings; /* 2 pointers */
	hlist_head_t  children; /* 1 pointer  */
	max_align_t   data[1];  /* not allocated, see below */
	
} hblock_t;

#define sizeof_hblock offsetof(hblock_t, data)

/*
 *	static methods
 */
static void * _realloc(void * ptr, size_t n);

static int  _relate(hblock_t * b, hblock_t * p);
static void _free_children(hblock_t * p);

/*
 *	Core API
 */
void * halloc(void * ptr, size_t len)
{
	hblock_t * p;

	/* calloc */
	if (! ptr)
	{
		if (! len)
			return NULL;

		p = _realloc(0, len + sizeof_hblock);
		if (! p)
			return NULL;
#ifndef NDEBUG
		p->magic = HH_MAGIC;
#endif
		hlist_init(&p->children);
		hlist_init_item(&p->siblings);

		return p->data;
	}

	p = structof(ptr, hblock_t, data);
	assert(p->magic == HH_MAGIC);

	/* realloc */
	if (len)
	{
		p = _realloc(p, len + sizeof_hblock);
		if (! p)
			return NULL;

		hlist_relink(&p->siblings);
		hlist_relink_head(&p->children);
		
		return p->data;
	}

	/* free */
	_free_children(p);
	hlist_del(&p->siblings);
	_realloc(p, 0);

	return NULL;
}

void hattach(void * block, void * parent)
{
	hblock_t * b, * p;
	
	if (! block)
	{
		assert(! parent);
		return;
	}

	/* detach */
	b = structof(block, hblock_t, data);
	assert(b->magic == HH_MAGIC);

	hlist_del(&b->siblings);

	if (! parent)
		return;

	/* attach */
	p = structof(parent, hblock_t, data);
	assert(p->magic == HH_MAGIC);
	
	/* sanity checks */
	assert(b != p);          /* trivial */
	assert(! _relate(p, b)); /* heavy ! */

	hlist_add(&p->children, &b->siblings);
}

/*
 *	malloc/free api
 */
void * h_malloc(size_t len)
{
	return halloc(0, len);
}

void * h_calloc(size_t n, size_t len)
{
	void * ptr = halloc(0, len*=n);
	return ptr ? memset(ptr, 0, len) : NULL;
}

void * h_realloc(void * ptr, size_t len)
{
	return halloc(ptr, len);
}

void   h_free(void * ptr)
{
	halloc(ptr, 0);
}

char * h_strdup(const char * str)
{
	size_t len = strlen(str);
	char * ptr = halloc(0, len + 1);
	return ptr ? (ptr[len] = 0, memcpy(ptr, str, len)) : NULL;
}

/*
 *	static stuff
 */
static void * _realloc(void * ptr, size_t n)
{
	/*
	 *	free'ing realloc()
	 */
	if (n)
		return realloc(ptr, n);
	free(ptr);
	return NULL;
}

static int _relate(hblock_t * b, hblock_t * p)
{
	hlist_item_t * i;

	if (!b || !p)
		return 0;

	/* 
	 *  since there is no 'parent' pointer, which would've allowed
	 *  O(log(n)) upward traversal, the check must use O(n) downward 
	 *  iteration of the entire hierarchy; and this can be VERY SLOW
	 */
	hlist_for_each(i, &p->children)
	{
		hblock_t * q = structof(i, hblock_t, siblings);
		if (q == b || _relate(b, q))
			return 1;
	}
	return 0;
}

static void _free_children(hblock_t * p)
{
	hlist_item_t * i, * tmp;
	
#ifndef NDEBUG
	/*
	 *	this catches loops in hierarchy with almost zero 
	 *	overhead (compared to _relate() running time)
	 */
	assert(p && p->magic == HH_MAGIC);
	p->magic = 0; 
#endif
	hlist_for_each_safe(i, tmp, &p->children)
	{
		hblock_t * q = structof(i, hblock_t, siblings);
		_free_children(q);
		_realloc(q, 0);
	}
}

