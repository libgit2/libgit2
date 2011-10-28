/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "repository.h"
#include "commit.h"

#define MAX_LOOPS 5
static const double max_load_factor = 0.65;

static int resize_to(git_hashtable *self, size_t new_size);
static int set_size(git_hashtable *self, size_t new_size);
static git_hashtable_node *node_with_hash(git_hashtable *self, const void *key, int hash_id);
static void node_swap_with(git_hashtable_node *self, git_hashtable_node *other);
static int node_insert(git_hashtable *self, git_hashtable_node *new_node);
static int insert_nodes(git_hashtable *self, git_hashtable_node *old_nodes, size_t old_size);

static int resize_to(git_hashtable *self, size_t new_size)
{
	git_hashtable_node *old_nodes = self->nodes;
	size_t old_size = self->size;

	self->is_resizing = 1;

	do {
		self->size = new_size;
		self->size_mask = new_size - 1;
		self->key_count = 0;
		self->nodes = git__calloc(1, sizeof(git_hashtable_node) * self->size);

		if (self->nodes == NULL)
			return GIT_ENOMEM;

		if (insert_nodes(self, old_nodes, old_size) == 0)
			self->is_resizing = 0;
		else {
			new_size *= 2;
			git__free(self->nodes);
		}
	} while(self->is_resizing);

	git__free(old_nodes);
	return GIT_SUCCESS;
}

static int set_size(git_hashtable *self, size_t new_size)
{
	self->nodes = git__realloc(self->nodes, new_size * sizeof(git_hashtable_node));
	if (self->nodes == NULL)
		return GIT_ENOMEM;

	if (new_size > self->size) {
		memset(&self->nodes[self->size], 0x0, (new_size - self->size) * sizeof(git_hashtable_node));
	}

	self->size = new_size;
	self->size_mask = new_size - 1;
	return GIT_SUCCESS;
}

static git_hashtable_node *node_with_hash(git_hashtable *self, const void *key, int hash_id)
{
	size_t pos = self->hash(key, hash_id) & self->size_mask;
	return git_hashtable_node_at(self->nodes, pos);
}

static void node_swap_with(git_hashtable_node *self, git_hashtable_node *other)
{
	git_hashtable_node tmp = *self;
	*self = *other;
	*other = tmp;
}

static int node_insert(git_hashtable *self, git_hashtable_node *new_node)
{
	int iteration, hash_id;

	for (iteration = 0; iteration < MAX_LOOPS; iteration++) {
		for (hash_id = 0; hash_id < GIT_HASHTABLE_HASHES; ++hash_id) {
			git_hashtable_node *node;
			node = node_with_hash(self, new_node->key, hash_id);
			node_swap_with(new_node, node);
			if(new_node->key == 0x0){
				self->key_count++;
				return GIT_SUCCESS;
			}
		}
	}

	if (self->is_resizing)
		return git__throw(GIT_EBUSY, "Failed to insert node. Hashtable is currently resizing");

	resize_to(self, self->size * 2);
	git_hashtable_insert(self, new_node->key, new_node->value);
	return GIT_SUCCESS;
}

static int insert_nodes(git_hashtable *self, git_hashtable_node *old_nodes, size_t old_size)
{
	size_t i;

	for (i = 0; i < old_size; ++i) {
		git_hashtable_node *node = git_hashtable_node_at(old_nodes, i);
		if (node->key && git_hashtable_insert(self, node->key, node->value) < GIT_SUCCESS)
			return GIT_ENOMEM;
	}

	return GIT_SUCCESS;
}

git_hashtable *git_hashtable_alloc(size_t min_size,
		git_hash_ptr hash,
		git_hash_keyeq_ptr key_eq)
{
	git_hashtable *table;

	assert(hash && key_eq);

	if ((table = git__malloc(sizeof(git_hashtable))) == NULL)
		return NULL;

	memset(table, 0x0, sizeof(git_hashtable));

	if (min_size < 8)
		min_size = 8;

	/* round up size to closest power of 2 */
	min_size--;
	min_size |= min_size >> 1;
	min_size |= min_size >> 2;
	min_size |= min_size >> 4;
	min_size |= min_size >> 8;
	min_size |= min_size >> 16;

	table->hash = hash;
	table->key_equal = key_eq;

	set_size(table, min_size + 1);

	return table;
}

void git_hashtable_clear(git_hashtable *self)
{
	assert(self);

	memset(self->nodes, 0x0, sizeof(git_hashtable_node) * self->size);
	self->key_count = 0;
}

void git_hashtable_free(git_hashtable *self)
{
	assert(self);

	git__free(self->nodes);
	git__free(self);
}


int git_hashtable_insert2(git_hashtable *self, const void *key, void *value, void **old_value)
{
	int hash_id;
	git_hashtable_node *node;

	assert(self && self->nodes);

	*old_value = NULL;

	for (hash_id = 0; hash_id < GIT_HASHTABLE_HASHES; ++hash_id) {
		node = node_with_hash(self, key, hash_id);

		if (!node->key) {
			node->key = key;
			node->value = value;
			self->key_count++;
			return GIT_SUCCESS;
		}

		if (key == node->key || self->key_equal(key, node->key) == 0) {
			*old_value = node->value;
			node->key = key;
			node->value = value;
			return GIT_SUCCESS;
		}
	}

	/* no space in table; must do cuckoo dance */
	{
		git_hashtable_node x;
		x.key = key;
		x.value = value;
		return node_insert(self, &x);
	}
}

void *git_hashtable_lookup(git_hashtable *self, const void *key)
{
	int hash_id;
	git_hashtable_node *node;

	assert(self && self->nodes);

	for (hash_id = 0; hash_id < GIT_HASHTABLE_HASHES; ++hash_id) {
		node = node_with_hash(self, key, hash_id);
		if (node->key && self->key_equal(key, node->key) == 0)
			return node->value;
	}

	return NULL;
}

int git_hashtable_remove(git_hashtable *self, const void *key)
{
	int hash_id;
	git_hashtable_node *node;

	assert(self && self->nodes);

	for (hash_id = 0; hash_id < GIT_HASHTABLE_HASHES; ++hash_id) {
		node = node_with_hash(self, key, hash_id);
		if (node->key && self->key_equal(key, node->key) == 0) {
			node->key = NULL;
			node->value = NULL;
			self->key_count--;
			return GIT_SUCCESS;
		}
	}

	return git__throw(GIT_ENOTFOUND, "Entry not found in hash table");
}

int git_hashtable_merge(git_hashtable *self, git_hashtable *other)
{
	if (resize_to(self, (self->size + other->size) * 2) < GIT_SUCCESS)
		return GIT_ENOMEM;

	return insert_nodes(self, other->nodes, other->key_count);
}

