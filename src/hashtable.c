/*
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2,
 * as published by the Free Software Foundation.
 *
 * In addition to the permissions in the GNU General Public License,
 * the authors give you unlimited permission to link the compiled
 * version of this file into combinations with other programs,
 * and to distribute those combinations without any restriction
 * coming from the use of this file.  (The General Public License
 * restrictions do apply in other respects; for example, they cover
 * modification of the file, and distribution when not linked into
 * a combined executable.)
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "common.h"
#include "repository.h"
#include "commit.h"

static const int default_table_size = 32;
static const double max_load_factor = 0.65;

static void hashtable_resize(git_hashtable *table)
{
	git_hashtable_node **new_nodes;
	unsigned int new_size, i;

	assert(table);

	new_size = (table->size_mask + 1) * 2;

	new_nodes = git__malloc(new_size * sizeof(git_hashtable_node *));
	if (new_nodes == NULL)
		return;

	memset(new_nodes, 0x0, new_size * sizeof(git_hashtable_node *));

	for (i = 0; i <= table->size_mask; ++i) {
		git_hashtable_node *n;
		unsigned int index;

		while ((n = table->nodes[i]) != NULL) {
			table->nodes[i] = n->next;
			index = n->hash & (new_size - 1);
			n->next = new_nodes[index];
			new_nodes[index] = n;
		}
	}

	free(table->nodes);
	table->nodes = new_nodes;
	table->size_mask = (new_size - 1);
	table->max_count = (unsigned int)(new_size * max_load_factor);
}

git_hashtable *git_hashtable_alloc(unsigned int min_size, 
		git_hash_ptr hash,
		git_hash_keyeq_ptr key_eq)
{
	unsigned int i;
	git_hashtable *table;

	assert(hash && key_eq);

	if ((table = git__malloc(sizeof(git_hashtable))) == NULL)
		return NULL;

	/* round up size to closest power of 2 */
	min_size--;
	min_size |= min_size >> 1;
	min_size |= min_size >> 2;
	min_size |= min_size >> 4;
	min_size |= min_size >> 8;
	min_size |= min_size >> 16;

	table->size_mask = min_size;
	table->count = 0;
	table->max_count = (unsigned int)((min_size + 1) * max_load_factor);

	table->hash = hash;
	table->key_equal = key_eq;

	table->nodes = git__malloc((min_size + 1) * sizeof(git_hashtable_node *));

	if (table->nodes == NULL) {
		free(table);
		return NULL;
	}

	for (i = 0; i <= min_size; ++i)
		table->nodes[i] = NULL;

	return table;
}

void git_hashtable_clear(git_hashtable *table)
{
	unsigned int index;

	assert(table);

	for (index = 0; index <= table->size_mask; ++index) {
		git_hashtable_node *node, *next_node;

		node = table->nodes[index];
		while (node != NULL) {
			next_node = node->next;
			free(node);
			node = next_node;
		}

		table->nodes[index] = NULL;
	}

	table->count = 0;
}

void git_hashtable_free(git_hashtable *table)
{
	assert(table);

	git_hashtable_clear(table);
	free(table->nodes);
	free(table);
}


int git_hashtable_insert(git_hashtable *table, const void *key, void *value)
{
	git_hashtable_node *node;
	uint32_t index, hash;

	assert(table);

	if (table->count + 1 > table->max_count)
		hashtable_resize(table);

	node = git__malloc(sizeof(git_hashtable_node));
	if (node == NULL)
		return GIT_ENOMEM;

	hash = table->hash(key);
	index = (hash & table->size_mask);

	node->object = value;
	node->hash = hash;
	node->next = table->nodes[index];

	table->nodes[index] = node;
	table->count++;

	return GIT_SUCCESS;
}

void *git_hashtable_lookup(git_hashtable *table, const void *key)
{
	git_hashtable_node *node;
	uint32_t index, hash;

	assert(table);

	hash = table->hash(key);
	index = (hash & table->size_mask);
	node = table->nodes[index];

	while (node != NULL) {
		if (node->hash == hash && table->key_equal(node->object, key))
			return node->object;

		node = node->next;
	}

	return NULL;
}

int git_hashtable_remove(git_hashtable *table, const void *key)
{
	git_hashtable_node *node, *prev_node;
	uint32_t index, hash;

	assert(table);

	hash = table->hash(key);
	index = (hash & table->size_mask);
	node = table->nodes[index];

	prev_node = NULL;

	while (node != NULL) {
		if (node->hash == hash && table->key_equal(node->object, key)) {
			if (prev_node == NULL)
				table->nodes[index] = node->next;
			else
				prev_node->next = node->next;

			free(node);
			return GIT_SUCCESS;
		}

		prev_node = node;
		node = node->next;
	}

	return GIT_ENOTFOUND;
}



void git_hashtable_iterator_init(git_hashtable *table, git_hashtable_iterator *it)
{
	assert(table && it);

	memset(it, 0x0, sizeof(git_hashtable_iterator));

	it->nodes = table->nodes;
	it->current_node = NULL;
	it->current_pos = 0;
	it->size = table->size_mask + 1;
}

void *git_hashtable_iterator_next(git_hashtable_iterator *it)
{
	git_hashtable_node *next = NULL;

	assert(it);

	while (it->current_node == NULL) {
		if (it->current_pos >= it->size)
			return NULL;

		it->current_node = it->nodes[it->current_pos++];
	}

	next = it->current_node;
	it->current_node = it->current_node->next;

	return next->object;
}

