/*
 * Copyright (C) 2009-2012 the libgit2 contributors
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
static void reinsert_stash(git_hashtable *self);

static int resize_to(git_hashtable *self, size_t new_size)
{
	git_hashtable_node *old_nodes = self->nodes;
	size_t old_size = self->size;
	git_hashtable_node old_stash[GIT_HASHTABLE_STASH_SIZE];
	size_t old_stash_count = self->stash_count;

	self->is_resizing = 1;

	if (old_stash_count > 0)
		memcpy(old_stash, self->stash,
			   old_stash_count * sizeof(git_hashtable_node));

	do {
		self->size = new_size;
		self->size_mask = new_size - 1;
		self->key_count = 0;
		self->stash_count = 0;
		self->nodes = git__calloc(1, sizeof(git_hashtable_node) * self->size);
		GITERR_CHECK_ALLOC(self->nodes);

		if (insert_nodes(self, old_nodes, old_size) == 0 &&
			insert_nodes(self, old_stash, old_stash_count) == 0)
			self->is_resizing = 0;
		else {
			new_size *= 2;
			git__free(self->nodes);
		}
	} while (self->is_resizing);

	git__free(old_nodes);
	return 0;
}

static int set_size(git_hashtable *self, size_t new_size)
{
	self->nodes =
		git__realloc(self->nodes, new_size * sizeof(git_hashtable_node));
	GITERR_CHECK_ALLOC(self->nodes);

	if (new_size > self->size)
		memset(&self->nodes[self->size], 0x0,
			(new_size - self->size) * sizeof(git_hashtable_node));

	self->size = new_size;
	self->size_mask = new_size - 1;
	return 0;
}

GIT_INLINE(git_hashtable_node *)node_with_hash(
	git_hashtable *self, const void *key, int hash_id)
{
	size_t pos = self->hash(key, hash_id) & self->size_mask;
	return git_hashtable_node_at(self->nodes, pos);
}

GIT_INLINE(void) node_swap_with(
	git_hashtable_node *self, git_hashtable_node *other)
{
	git_hashtable_node tmp = *self;
	*self = *other;
	*other = tmp;
}

static int node_insert(git_hashtable *self, git_hashtable_node *new_node)
{
	int iteration, hash_id;
	git_hashtable_node *node;

	for (iteration = 0; iteration < MAX_LOOPS; iteration++) {
		for (hash_id = 0; hash_id < GIT_HASHTABLE_HASHES; ++hash_id) {
			node = node_with_hash(self, new_node->key, hash_id);
			node_swap_with(new_node, node);
			if (new_node->key == 0x0) {
				self->key_count++;
				return 0;
			}
		}
	}

	/* Insert into stash if there is space */
	if (self->stash_count < GIT_HASHTABLE_STASH_SIZE) {
		node_swap_with(new_node, &self->stash[self->stash_count++]);
		self->key_count++;
		return 0;
	}

	/* Failed to insert node. Hashtable is currently resizing */
	assert(!self->is_resizing);

	if (resize_to(self, self->size * 2) < 0)
		return -1;

	return git_hashtable_insert(self, new_node->key, new_node->value);
}

static int insert_nodes(
	git_hashtable *self, git_hashtable_node *old_nodes, size_t old_size)
{
	size_t i;

	for (i = 0; i < old_size; ++i) {
		git_hashtable_node *node = git_hashtable_node_at(old_nodes, i);
		if (node->key && node_insert(self, node) < 0)
			return -1;
	}

	return 0;
}

static void reinsert_stash(git_hashtable *self)
{
	int stash_count;
	struct git_hashtable_node stash[GIT_HASHTABLE_STASH_SIZE];

	if (self->stash_count <= 0)
		return;

	memcpy(stash, self->stash, self->stash_count * sizeof(git_hashtable_node));
	stash_count = self->stash_count;
	self->stash_count = 0;

	/* the node_insert() calls *cannot* fail because the stash is empty */
	insert_nodes(self, stash, stash_count);
}

git_hashtable *git_hashtable_alloc(
	size_t min_size,
	git_hash_ptr hash,
	git_hash_keyeq_ptr key_eq)
{
	git_hashtable *table;

	assert(hash && key_eq);

	if ((table = git__malloc(sizeof(*table))) == NULL)
		return NULL;

	memset(table, 0x0, sizeof(git_hashtable));

	table->hash = hash;
	table->key_equal = key_eq;

	min_size = git__size_t_powerof2(min_size < 8 ? 8 : min_size);
	set_size(table, min_size);

	return table;
}

void git_hashtable_clear(git_hashtable *self)
{
	assert(self);

	memset(self->nodes, 0x0, sizeof(git_hashtable_node) * self->size);

	self->stash_count = 0;
	self->key_count = 0;
}

void git_hashtable_free(git_hashtable *self)
{
	assert(self);

	git__free(self->nodes);
	git__free(self);
}


int git_hashtable_insert2(
	git_hashtable *self, const void *key, void *value, void **old_value)
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
			return 0;
		}

		if (key == node->key || self->key_equal(key, node->key) == 0) {
			*old_value = node->value;
			node->key = key;
			node->value = value;
			return 0;
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

static git_hashtable_node *find_node(git_hashtable *self, const void *key)
{
	int hash_id, count = 0;
	git_hashtable_node *node;

	for (hash_id = 0; hash_id < GIT_HASHTABLE_HASHES; ++hash_id) {
		node = node_with_hash(self, key, hash_id);
		if (node->key) {
			++count;
			if (self->key_equal(key, node->key) == 0)
				return node;
		}
	}

	/* check stash if not found but all slots were filled */
	if (count == GIT_HASHTABLE_HASHES) {
		for (count = 0; count < self->stash_count; ++count)
			if (self->key_equal(key, self->stash[count].key) == 0)
				return &self->stash[count];
	}

	return NULL;
}

static void reset_stash(git_hashtable *self, git_hashtable_node *node)
{
	/* if node was in stash, then compact stash */
	ssize_t offset = node - self->stash;

	if (offset >= 0 && offset < self->stash_count) {
		if (offset < self->stash_count - 1)
			memmove(node, node + 1, (self->stash_count - offset) *
					sizeof(git_hashtable_node));
		self->stash_count--;
	}

	reinsert_stash(self);
}

void *git_hashtable_lookup(git_hashtable *self, const void *key)
{
	git_hashtable_node *node;
	assert(self && key);
	node = find_node(self, key);
	return node ? node->value : NULL;
}

int git_hashtable_remove2(
	git_hashtable *self, const void *key, void **old_value)
{
	git_hashtable_node *node;

	assert(self && self->nodes);

	node = find_node(self, key);
	if (node) {
		*old_value = node->value;

		node->key = NULL;
		node->value = NULL;
		self->key_count--;

		reset_stash(self, node);
		return 0;
	}

	return GIT_ENOTFOUND;
}

int git_hashtable_merge(git_hashtable *self, git_hashtable *other)
{
	size_t new_size = git__size_t_powerof2(self->size + other->size);

	if (resize_to(self, new_size) < 0)
		return -1;

	if (insert_nodes(self, other->nodes, other->key_count) < 0)
		return -1;

	return insert_nodes(self, other->stash, other->stash_count);
}


/**
 * Standard string
 */
uint32_t git_hash__strhash_cb(const void *key, int hash_id)
{
	static uint32_t hash_seeds[GIT_HASHTABLE_HASHES] = {
		2147483647,
		0x5d20bb23,
		0x7daaab3c
	};

	size_t key_len = strlen((const char *)key);

	/* won't take hash of strings longer than 2^31 right now */
	assert(key_len == (size_t)((int)key_len));

	return git__hash(key, (int)key_len, hash_seeds[hash_id]);
}
