#ifndef INCLUDE_hashtable_h__
#define INCLUDE_hashtable_h__

#include "git2/common.h"
#include "git2/oid.h"
#include "git2/odb.h"
#include "common.h"

#define GIT_HASHTABLE_HASHES 3

typedef uint32_t (*git_hash_ptr)(const void *, int hash_id);
typedef int (*git_hash_keyeq_ptr)(const void *key_a, const void *key_b);

struct git_hashtable_node {
	const void *key;
	void *value;
};

struct git_hashtable {
	struct git_hashtable_node *nodes;

	size_t size_mask;
	size_t size;
	size_t key_count;

	int is_resizing;

	git_hash_ptr hash;
	git_hash_keyeq_ptr key_equal;
};

typedef struct git_hashtable_node git_hashtable_node;
typedef struct git_hashtable git_hashtable;

git_hashtable *git_hashtable_alloc(size_t min_size,
		git_hash_ptr hash,
		git_hash_keyeq_ptr key_eq);
void *git_hashtable_lookup(git_hashtable *h, const void *key);
int git_hashtable_remove(git_hashtable *table, const void *key);
void git_hashtable_free(git_hashtable *h);
void git_hashtable_clear(git_hashtable *h);
int git_hashtable_merge(git_hashtable *self, git_hashtable *other);

int git_hashtable_insert2(git_hashtable *h, const void *key, void *value, void **old_value);

GIT_INLINE(int) git_hashtable_insert(git_hashtable *h, const void *key, void *value)
{
	void *_unused;
	return git_hashtable_insert2(h, key, value, &_unused);
}

#define git_hashtable_node_at(nodes, pos) ((git_hashtable_node *)(&nodes[pos]))

#define GIT_HASHTABLE_FOREACH(self, pkey, pvalue, code) {\
	git_hashtable *_self = (self);\
	git_hashtable_node *_nodes = _self->nodes;\
	unsigned int _i, _size = _self->size;\
	for (_i = 0; _i < _size; _i ++) {\
		git_hashtable_node *_node = git_hashtable_node_at(_nodes, _i);\
		if (_node->key)\
		{\
			pkey = _node->key;\
			pvalue = _node->value;\
			code;\
		}\
	}\
}

#define GIT_HASHTABLE_FOREACH_DELETE() {\
	_node->key = NULL; _node->value = NULL; _self->key_count--;\
}


#endif
