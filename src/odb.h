#ifndef INCLUDE_odb_h__
#define INCLUDE_odb_h__

#include "git2/odb.h"
#include "git2/oid.h"
#include "git2/types.h"

#include "vector.h"
#include "cache.h"

/* DO NOT EXPORT */
typedef struct {
	void *data;          /**< Raw, decompressed object data. */
	size_t len;          /**< Total number of bytes in data. */
	git_otype type;      /**< Type of this object. */
} git_rawobj;

/* EXPORT */
struct git_odb_object {
	git_cached_obj cached;
	git_rawobj raw;
};

/* EXPORT */
struct git_odb {
	void *_internal;
	git_vector backends;
	git_cache cache;
};

int git_odb__hash_obj(git_oid *id, git_rawobj *obj);

#endif
