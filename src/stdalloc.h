/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#ifndef INCLUDE_stdalloc_h__
#define INCLUDE_stdalloc_h__

#include "common.h"

/*
 * Custom memory allocation wrappers
 * that set error code and error message
 * on allocation failure
 */
void *git__stdalloc__malloc(size_t len, const char *file, int line);
void *git__stdalloc__calloc(size_t nelem, size_t elsize, const char *file, int line);
char *git__stdalloc__strdup(const char *str, const char *file, int line);
char *git__stdalloc__strndup(const char *str, size_t n, const char *file, int line);
/* NOTE: This doesn't do null or '\0' checking.  Watch those boundaries! */
char *git__stdalloc__substrdup(const char *start, size_t n, const char *file, int line);
void *git__stdalloc__realloc(void *ptr, size_t size, const char *file, int line);

/**
 * Similar to `git__stdalloc__realloc`, except that it is suitable for reallocing an
 * array to a new number of elements of `nelem`, each of size `elsize`.
 * The total size calculation is checked for overflow.
 */
void *git__stdalloc__reallocarray(void *ptr, size_t nelem, size_t elsize, const char *file, int line);

/**
 * Similar to `git__stdalloc__calloc`, except that it does not zero memory.
 */
void *git__stdalloc__mallocarray(size_t nelem, size_t elsize, const char *file, int line);

void git__stdalloc__free(void *ptr);

#endif
