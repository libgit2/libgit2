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
#include "revobject.h"

static const double max_load_factor = 0.65;

static unsigned int git_revpool_table__hash(const git_oid *id)
{
	unsigned int r;
	memcpy(&r, id->id, sizeof(r));
	return r;
}

git_revpool_table *git_revpool_table_create(unsigned int min_size)
{
	git_revpool_table *table;
	unsigned int i;

	table = git__malloc(sizeof(*table));

	if (table == NULL)
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

	table->nodes = git__malloc((min_size + 1) * sizeof(git_revpool_node *));

	if (table->nodes == NULL) {
		free(table);
		return NULL;
	}

	for (i = 0; i <= min_size; ++i)
		table->nodes[i] = NULL;

	return table;
}

int git_revpool_table_insert(git_revpool_table *table, git_revpool_object *object)
{
	git_revpool_node *node;
	unsigned int index, hash;

	if (table == NULL)
		return GIT_ERROR;

	if (table->count + 1 > table->max_count)
		git_revpool_table_resize(table);

	node = git__malloc(sizeof(git_revpool_node));
	if (node == NULL)
		return GIT_ENOMEM;

	hash = git_revpool_table__hash(&object->id);
	index = (hash & table->size_mask);

	node->object = object;
	node->hash = hash;
	node->next = table->nodes[index];

	table->nodes[index] = node;
	table->count++;

	return 0;
}

git_revpool_object *git_revpool_table_lookup(git_revpool_table *table, const git_oid *id)
{
	git_revpool_node *node;
	unsigned int index, hash;

	if (table == NULL)
		return NULL;

	hash = git_revpool_table__hash(id);
	index = (hash & table->size_mask);
	node = table->nodes[index];

	while (node != NULL) {
		if (node->hash == hash && git_oid_cmp(id, &node->object->id) == 0)
			return node->object;

		node = node->next;
	}

	return NULL;
}

void git_revpool_table_resize(git_revpool_table *table)
{
	git_revpool_node **new_nodes;
	unsigned int new_size, i;

	new_size = (table->size_mask + 1) * 2;

	new_nodes = git__malloc(new_size * sizeof(git_revpool_node *));
	if (new_nodes == NULL)
		return;

	memset(new_nodes, 0x0, new_size * sizeof(git_revpool_node *));

	for (i = 0; i <= table->size_mask; ++i) {
		git_revpool_node *n;
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


void git_revpool_table_free(git_revpool_table *table)
{
	unsigned int index;

	for (index = 0; index <= table->size_mask; ++index) {
		git_revpool_node *node, *next_node;

		node = table->nodes[index];
		while (node != NULL) {
			next_node = node->next;
			free(node);
			node = next_node;
		}
	}

	free(table->nodes);
	free(table);
}

void git_revpool_tableit_init(git_revpool_table *table, git_revpool_tableit *it)
{
	memset(it, 0x0, sizeof(git_revpool_tableit));

	it->nodes = table->nodes;
	it->current_node = NULL;
	it->current_pos = 0;
	it->size = table->size_mask + 1;
}

git_revpool_object *git_revpool_tableit_next(git_revpool_tableit *it)
{
	git_revpool_node *next = NULL;

	while (it->current_node == NULL) {
		if (it->current_pos >= it->size)
			return NULL;

		it->current_node = it->nodes[it->current_pos++];
	}

	next = it->current_node;
	it->current_node = it->current_node->next;

	return next->object;
}
