/*
Copyright 2020 Google LLC

Use of this source code is governed by a BSD-style
license that can be found in the LICENSE file or at
https://developers.google.com/open-source/licenses/bsd
*/

#include "reftable.h"

#include "basics.h"
#include "system.h"

const char *reftable_error_str(int err)
{
	static char buf[250];
	switch (err) {
	case REFTABLE_IO_ERROR:
		return "I/O error";
	case REFTABLE_FORMAT_ERROR:
		return "corrupt reftable file";
	case REFTABLE_NOT_EXIST_ERROR:
		return "file does not exist";
	case REFTABLE_LOCK_ERROR:
		return "data is outdated";
	case REFTABLE_API_ERROR:
		return "misuse of the reftable API";
	case REFTABLE_ZLIB_ERROR:
		return "zlib failure";
	case REFTABLE_NAME_CONFLICT:
		return "file/directory conflict";
	case REFTABLE_REFNAME_ERROR:
		return "invalid refname";
	case -1:
		return "general error";
	default:
		snprintf(buf, sizeof(buf), "unknown error code %d", err);
		return buf;
	}
}

int reftable_error_to_errno(int err)
{
	switch (err) {
	case REFTABLE_IO_ERROR:
		return EIO;
	case REFTABLE_FORMAT_ERROR:
		return EFAULT;
	case REFTABLE_NOT_EXIST_ERROR:
		return ENOENT;
	case REFTABLE_LOCK_ERROR:
		return EBUSY;
	case REFTABLE_API_ERROR:
		return EINVAL;
	case REFTABLE_ZLIB_ERROR:
		return EDOM;
	default:
		return ERANGE;
	}
}

void *(*reftable_malloc_ptr)(size_t sz) = &malloc;
void *(*reftable_realloc_ptr)(void *, size_t) = &realloc;
void (*reftable_free_ptr)(void *) = &free;

void *reftable_malloc(size_t sz)
{
	return (*reftable_malloc_ptr)(sz);
}

void *reftable_realloc(void *p, size_t sz)
{
	return (*reftable_realloc_ptr)(p, sz);
}

void reftable_free(void *p)
{
	reftable_free_ptr(p);
}

void *reftable_calloc(size_t sz)
{
	void *p = reftable_malloc(sz);
	memset(p, 0, sz);
	return p;
}

void reftable_set_alloc(void *(*malloc)(size_t),
			void *(*realloc)(void *, size_t), void (*free)(void *))
{
	reftable_malloc_ptr = malloc;
	reftable_realloc_ptr = realloc;
	reftable_free_ptr = free;
}

int reftable_fd_write(void *arg, const void *data, size_t sz)
{
	int *fdp = (int *)arg;
	return write(*fdp, data, sz);
}
