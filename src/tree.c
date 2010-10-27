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
#include "commit.h"
#include "revwalk.h"
#include "tree.h"
#include "git/repository.h"

static int resize_tree_array(git_tree *tree)
{
	git_tree_entry **new_entries;

	tree->array_size *= 2; 
	if (tree->array_size == 0)
		tree->array_size = 8;

	new_entries = git__malloc(tree->array_size * sizeof(git_tree_entry *));
	if (new_entries == NULL)
		return GIT_ENOMEM;

	memcpy(new_entries, tree->entries, tree->entry_count * sizeof(git_tree_entry *));

	free(tree->entries);
	tree->entries = new_entries;

	return GIT_SUCCESS;
}

int entry_cmp(const void *key, const void *array_member)
{
	const char *filename = (const char *)key;
	const git_tree_entry *entry = *(const git_tree_entry **)(array_member);

	return strcmp(filename, entry->filename);
}

int entry_sort_cmp(const void *a, const void *b)
{
	const git_tree_entry *entry_a = *(const git_tree_entry **)(a);
	const git_tree_entry *entry_b = *(const git_tree_entry **)(b);

	return strcmp(entry_a->filename, entry_b->filename);
}

static void entry_resort(git_tree *tree)
{
	qsort(tree->entries, tree->entry_count, sizeof(git_tree_entry *), entry_sort_cmp);
}

static void free_tree_entries(git_tree *tree)
{
	size_t i;

	if (tree == NULL)
		return;

	for (i = 0; i < tree->entry_count; ++i) {
		free(tree->entries[i]->filename);
		free(tree->entries[i]);
	}

	free(tree->entries);
}



void git_tree__free(git_tree *tree)
{
	free_tree_entries(tree);
	free(tree);
}

git_tree *git_tree_new(git_repository *repo)
{
	return (git_tree *)git_object_new(repo, GIT_OBJ_TREE);
}

const git_oid *git_tree_id(git_tree *c)
{
	return git_object_id((git_object *)c);
}

git_tree *git_tree_lookup(git_repository *repo, const git_oid *id)
{
	return (git_tree *)git_repository_lookup(repo, id, GIT_OBJ_TREE);
}

void git_tree_entry_set_attributes(git_tree_entry *entry, int attr)
{
	assert(entry && entry->owner);

	entry->attr = attr;
	entry->owner->object.modified = 1;
}

void git_tree_entry_set_name(git_tree_entry *entry, const char *name)
{
	assert(entry && entry->owner);

	free(entry->filename);
	entry->filename = git__strdup(name);
	entry_resort(entry->owner);
	entry->owner->object.modified = 1;
}

void git_tree_entry_set_id(git_tree_entry *entry, const git_oid *oid)
{
	assert(entry && entry->owner);

	git_oid_cpy(&entry->oid, oid);
	entry->owner->object.modified = 1;
}

unsigned int git_tree_entry_attributes(git_tree_entry *entry)
{
	return entry->attr;
}

const char *git_tree_entry_name(git_tree_entry *entry)
{
	assert(entry);
	return entry->filename;
}

const git_oid *git_tree_entry_id(git_tree_entry *entry)
{
	assert(entry);
	return &entry->oid;
}

git_object *git_tree_entry_2object(git_tree_entry *entry)
{
	assert(entry);
	return git_repository_lookup(entry->owner->object.repo, &entry->oid, GIT_OBJ_ANY);
}

git_tree_entry *git_tree_entry_byname(git_tree *tree, const char *filename)
{
	git_tree_entry **found;

	assert(tree && filename);

	found = bsearch(filename, tree->entries, tree->entry_count, sizeof(git_tree_entry *), entry_cmp);
	return found ? *found : NULL;
}

git_tree_entry *git_tree_entry_byindex(git_tree *tree, int idx)
{
	assert(tree);

	if (tree->entries == NULL)
		return NULL;

	return (idx >= 0 && idx < (int)tree->entry_count) ? tree->entries[idx] : NULL;
}

size_t git_tree_entrycount(git_tree *tree)
{
	assert(tree);
	return tree->entry_count;
}

int git_tree_add_entry(git_tree *tree, const git_oid *id, const char *filename, int attributes)
{
	git_tree_entry *entry;

	assert(tree && id && filename);

	if (tree->entry_count >= tree->array_size)
		if (resize_tree_array(tree) < 0)
			return GIT_ENOMEM;

	if ((entry = git__malloc(sizeof(git_tree_entry))) == NULL)
		return GIT_ENOMEM;

	memset(entry, 0x0, sizeof(git_tree_entry));

	entry->filename = git__strdup(filename);
	git_oid_cpy(&entry->oid, id);
	entry->attr = attributes;
	entry->owner = tree;

	tree->entries[tree->entry_count++] = entry;
	entry_resort(tree);

	tree->object.modified = 1;
	return GIT_SUCCESS;
}

int git_tree_remove_entry_byindex(git_tree *tree, int idx)
{
	git_tree_entry *remove_ptr;

	assert(tree);

	if (idx < 0 || idx >= (int)tree->entry_count)
		return GIT_ENOTFOUND;

	remove_ptr = tree->entries[idx];
	tree->entries[idx] = tree->entries[--tree->entry_count];

	free(remove_ptr->filename);
	free(remove_ptr);
	entry_resort(tree);

	tree->object.modified = 1;
	return GIT_SUCCESS;
}

int git_tree_remove_entry_byname(git_tree *tree, const char *filename)
{
	git_tree_entry **entry_ptr;
	int idx;

	assert(tree && filename);

	entry_ptr = bsearch(filename, tree->entries, tree->entry_count, sizeof(git_tree_entry *), entry_cmp);
	if (entry_ptr == NULL)
		return GIT_ENOTFOUND;

	idx = (int)(entry_ptr - tree->entries);
	return git_tree_remove_entry_byindex(tree, idx);
}

int git_tree__writeback(git_tree *tree, git_odb_source *src)
{
	size_t i;
	char filemode[8];

	assert(tree && src);

	if (tree->entries == NULL)
		return GIT_ERROR;

	entry_resort(tree);

	for (i = 0; i < tree->entry_count; ++i) {
		git_tree_entry *entry;
		entry = tree->entries[i];
	
		sprintf(filemode, "%06o ", entry->attr);

		git__source_write(src, filemode, strlen(filemode));
		git__source_write(src, entry->filename, strlen(entry->filename) + 1);
		git__source_write(src, entry->oid.id, GIT_OID_RAWSZ);
	} 

	return GIT_SUCCESS;
}


static int tree_parse_buffer(git_tree *tree, char *buffer, char *buffer_end)
{
	static const size_t avg_entry_size = 40;
	int error = 0;

	free_tree_entries(tree);

	tree->entry_count = 0;
	tree->array_size = (tree->object.source.raw.len / avg_entry_size) + 1;
	tree->entries = git__malloc(tree->array_size * sizeof(git_tree_entry *));

	if (tree->entries == NULL)
		return GIT_ENOMEM;

	while (buffer < buffer_end) {
		git_tree_entry *entry;

		if (tree->entry_count >= tree->array_size)
			if (resize_tree_array(tree) < 0)
				return GIT_ENOMEM;

		entry = git__malloc(sizeof(git_tree_entry));
		if (entry == NULL) {
			error = GIT_ENOMEM;
			break;
		}

		tree->entries[tree->entry_count++] = entry;

		entry->owner = tree;
		entry->attr = strtol(buffer, &buffer, 8);

		if (*buffer++ != ' ') {
			error = GIT_EOBJCORRUPTED;
			break;
		}

		if (memchr(buffer, 0, buffer_end - buffer) == NULL) {
			error = GIT_EOBJCORRUPTED;
			break;
		}

		entry->filename = git__strdup(buffer);

		while (buffer < buffer_end && *buffer != 0)
			buffer++;

		buffer++;

		git_oid_mkraw(&entry->oid, (const unsigned char *)buffer);
		buffer += GIT_OID_RAWSZ;
	}

	return error;
}

int git_tree__parse(git_tree *tree)
{
	char *buffer, *buffer_end;

	assert(tree && tree->object.source.open);
	assert(!tree->object.in_memory);

	buffer = tree->object.source.raw.data;
	buffer_end = buffer + tree->object.source.raw.len;

	return tree_parse_buffer(tree, buffer, buffer_end);
}

