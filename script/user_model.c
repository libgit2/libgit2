/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

void *realloc(void *ptr, size_t size);
size_t strlen(const char *s);

typedef struct git_vector {
	void **contents;
	size_t length;
} git_vector;

typedef struct git_buf {
	char *ptr;
	size_t asize, size;
} git_buf;

int git_vector_insert(git_vector *v, void *element)
{
	if (!v)
		__coverity_panic__();

	v->contents = realloc(v->contents, ++v->length);
	if (!v->contents)
		__coverity_panic__();
	v->contents[v->length] = element;

	return 0;
}

int git_buf_len(const struct git_buf *buf)
{
	return strlen(buf->ptr);
}
