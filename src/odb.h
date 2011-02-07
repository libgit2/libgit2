#ifndef INCLUDE_odb_h__
#define INCLUDE_odb_h__

#include "git2/odb.h"
#include "git2/oid.h"

#include "vector.h"

struct git_odb {
	void *_internal;
	git_vector backends;
};

int git_odb__hash_obj(git_oid *id, char *hdr, size_t n, int *len, git_rawobj *obj);
int git_odb__inflate_buffer(void *in, size_t inlen, void *out, size_t outlen);

#endif
