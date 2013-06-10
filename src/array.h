/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_array_h__
#define INCLUDE_array_h__

#include "util.h"

#define git_array_t(type) struct { type *ptr; size_t size, asize; }

#define git_array_init(a) \
	do { (a).size = (a).asize = 0; (a).ptr = NULL; } while (0)

#define git_array_clear(a) \
	do { git__free((a).ptr); git_array_init(a); } while (0)

#define git_array_grow(a) do { \
	void *new_array; size_t new_size = \
	((a).asize >= 256) ? (a).asize + 256 : ((a).asize >= 8) ? (a).asize * 2 : 8; \
	new_array = git__realloc((a).ptr, new_size * sizeof(*(a).ptr)); \
	if (!new_array) { git_array_clear(a); } \
	else { (a).ptr = new_array; (a).asize = new_size; } \
	} while (0)

#define GITERR_CHECK_ARRAY(a) GITERR_CHECK_ALLOC((a).ptr)

#define git_array_alloc(a, el) do { \
	if ((a).size >= (a).asize) git_array_grow(a); \
	(el) = (a).ptr ? &(a).ptr[(a).size++] : NULL; \
	} while (0)

#define git_array_last(a) ((a).size ? &(a).ptr[(a).size - 1] : NULL)

#define git_array_get(a, i) (((i) < (a).size) ? &(a).ptr[(i)] : NULL)

#define git_array_size(a) (a).size

#endif
