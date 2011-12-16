/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_attr_h__
#define INCLUDE_attr_h__

#include "hashtable.h"
#include "attr_file.h"

/* EXPORT */
typedef struct {
	git_hashtable *files;		/* hash path to git_attr_file */
} git_attr_cache;

extern void git_repository__attr_cache_free(git_attr_cache *attrs);

#endif

