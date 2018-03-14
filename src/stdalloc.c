/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "stdalloc.h"

void *git__stdalloc__malloc(size_t len, const char *file, int line)
{
	void *ptr = malloc(len);

	GIT_UNUSED(file);
	GIT_UNUSED(line);

	if (!ptr) giterr_set_oom();
	return ptr;
}

void *git__stdalloc__calloc(size_t nelem, size_t elsize, const char *file, int line)
{
	void *ptr = calloc(nelem, elsize);

	GIT_UNUSED(file);
	GIT_UNUSED(line);

	if (!ptr) giterr_set_oom();
	return ptr;
}

char *git__stdalloc__strdup(const char *str, const char *file, int line)
{
	char *ptr = strdup(str);

	GIT_UNUSED(file);
	GIT_UNUSED(line);

	if (!ptr) giterr_set_oom();
	return ptr;
}

char *git__stdalloc__strndup(const char *str, size_t n, const char *file, int line)
{
	size_t length = 0, alloclength;
	char *ptr;

	length = p_strnlen(str, n);

	if (GIT_ADD_SIZET_OVERFLOW(&alloclength, length, 1) ||
		!(ptr = git__stdalloc__malloc(alloclength, file, line)))
		return NULL;

	if (length)
		memcpy(ptr, str, length);

	ptr[length] = '\0';

	return ptr;
}

char *git__stdalloc__substrdup(const char *start, size_t n, const char *file, int line)
{
	char *ptr;
	size_t alloclen;

	if (GIT_ADD_SIZET_OVERFLOW(&alloclen, n, 1) ||
		!(ptr = git__stdalloc__malloc(alloclen, file, line)))
		return NULL;

	memcpy(ptr, start, n);
	ptr[n] = '\0';
	return ptr;
}

void *git__stdalloc__realloc(void *ptr, size_t size, const char *file, int line)
{
	void *new_ptr = realloc(ptr, size);

	GIT_UNUSED(file);
	GIT_UNUSED(line);

	if (!new_ptr) giterr_set_oom();
	return new_ptr;
}

void *git__stdalloc__reallocarray(void *ptr, size_t nelem, size_t elsize, const char *file, int line)
{
	size_t newsize;

	GIT_UNUSED(file);
	GIT_UNUSED(line);

	return GIT_MULTIPLY_SIZET_OVERFLOW(&newsize, nelem, elsize) ?
		NULL : realloc(ptr, newsize);
}

void *git__stdalloc__mallocarray(size_t nelem, size_t elsize, const char *file, int line)
{
	return git__stdalloc__reallocarray(NULL, nelem, elsize, file, line);
}

void git__stdalloc__free(void *ptr)
{
	free(ptr);
}
