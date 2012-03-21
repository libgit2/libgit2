/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_diff_h__
#define INCLUDE_diff_h__

#include <stdio.h>
#include "vector.h"
#include "buffer.h"
#include "iterator.h"
#include "repository.h"

struct git_diff_list {
	git_repository   *repo;
	git_diff_options opts;
	git_vector       deltas;    /* vector of git_diff_file_delta */
	git_iterator_type_t old_src;
	git_iterator_type_t new_src;
};

/* macro lets you iterate over two diff lists together */

#define GIT_DIFF_COITERATE(A,B,AD,BD,LEFT,RIGHT,BOTH,AFTER) do { \
	unsigned int _i = 0, _j = 0; int _cmp; \
	while (((A) && _i < (A)->deltas.length) || ((B) && _j < (B)->deltas.length)) { \
		(AD) = (A) ? GIT_VECTOR_GET(&(A)->deltas,_i) : NULL; \
		(BD) = (B) ? GIT_VECTOR_GET(&(B)->deltas,_j) : NULL; \
		_cmp = !(BD) ? -1 : !(AD) ? 1 : strcmp((AD)->old.path,(BD)->old.path); \
		if (_cmp < 0) { LEFT; _i++; } \
		else if (_cmp > 0) { RIGHT; _j++; } \
		else { BOTH; _i++; _j++; } \
		AFTER; \
	} } while (0)

#endif

