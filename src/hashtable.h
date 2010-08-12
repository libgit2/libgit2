#ifndef INCLUDE_hashtable_h__
#define INCLUDE_hashtable_h__

#include "git/common.h"
#include "git/oid.h"
#include "git/odb.h"


typedef uint32_t (*git_hash_ptr)(const void *);
typedef int (*git_hash_keyeq_ptr)(void *obj, const void *obj_key);

struct git_hashtable_node {
	void *object;
	uint32_t hash;
	struct git_hashtable_node *next;
};

struct git_hashtable {
	struct git_hashtable_node **nodes;

	unsigned int size_mask;
	unsigned int count;
	unsigned int max_count;

	git_hash_ptr hash;
	git_hash_keyeq_ptr key_equal;
};

struct git_hashtable_iterator {
	struct git_hashtable_node **nodes;
	struct git_hashtable_node *current_node;
	unsigned int current_pos;
	unsigned int size;
};

typedef struct git_hashtable_node git_hashtable_node;
typedef struct git_hashtable git_hashtable;
typedef struct git_hashtable_iterator git_hashtable_iterator;

git_hashtable *git_hashtable_alloc(unsigned int min_size, 
		git_hash_ptr hash,
		git_hash_keyeq_ptr key_eq);
int git_hashtable_insert(git_hashtable *h, const void *key, void *value);
void *git_hashtable_lookup(git_hashtable *h, const void *key);
int git_hashtable_remove(git_hashtable *table, const void *key);
void git_hashtable_free(git_hashtable *h);
void git_hashtable_clear(git_hashtable *h);

void *git_hashtable_iterator_next(git_hashtable_iterator *it);
void git_hashtable_iterator_init(git_hashtable *h, git_hashtable_iterator *it);

#endif
