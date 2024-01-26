


#ifndef BASECACHE_H
#define BASECACHE_H

#include "sizemap.h"

#include "git2_util.h"

typedef struct basecache_entry basecache_entry;

struct basecache_entry {
	struct object_data *data;
	git_object_size_t position;
	size_t size;
	basecache_entry *prev;
	basecache_entry *next;
};

struct basecache {
	git_sizemap *map;
	git_rwlock lock;
	size_t size;
	size_t used;
	basecache_entry *oldest;
	basecache_entry *newest;
};

extern int basecache_init(struct basecache *bc);
extern struct object_data *basecache_get(
	struct basecache *bc,
	git_object_size_t position);
extern int basecache_put(
	struct basecache *bc,
	git_object_size_t position,
	struct object_data *data);
extern void basecache_dispose(struct basecache *bc);

#endif
