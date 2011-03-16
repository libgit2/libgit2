#ifndef INCLUDE_cache_h__
#define INCLUDE_cache_h__

#include "git2/common.h"
#include "git2/oid.h"
#include "git2/odb.h"

#include "thread-utils.h"

typedef void (*git_cached_obj_freeptr)(void *);

typedef struct {
	git_oid oid;
	git_atomic refcount;
} git_cached_obj;

typedef struct {
	git_cached_obj *ptr;
	git_mutex lock;
	unsigned int lru;
} cache_node;

typedef struct {
	cache_node *nodes;

	unsigned int lru_count;
	size_t size_mask;
	git_cached_obj_freeptr free_obj;
} git_cache;


void git_cache_init(git_cache *cache, size_t size, git_cached_obj_freeptr free_ptr);
void git_cache_free(git_cache *cache);

void *git_cache_try_store(git_cache *cache, void *entry);
void *git_cache_get(git_cache *cache, const git_oid *oid);

#endif
