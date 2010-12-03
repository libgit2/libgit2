#ifndef INCLUDE_odb_h__
#define INCLUDE_odb_h__

#include <git/odb.h>
#include <git/oid.h>

#include "vector.h"

struct git_odb {
	void *_internal;
	git_vector backends;
};

int git_odb__hash_obj(git_oid *id, char *hdr, size_t n, int *len, git_rawobj *obj);
int git_odb__inflate_buffer(void *in, size_t inlen, void *out, size_t outlen);


int git_odb_backend_loose(git_odb_backend **backend_out, const char *objects_dir);
int git_odb_backend_pack(git_odb_backend **backend_out, const char *objects_dir);

#endif
