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

static void resize_tree_array(git_tree *tree)
{
	git_tree_entry **new_entries;

	tree->array_size = tree->array_size * 2;

	new_entries = git__malloc(tree->array_size * sizeof(git_tree_entry *));
	memcpy(new_entries, tree->entries, tree->entry_count * sizeof(git_tree_entry *));

	free(tree->entries);
	tree->entries = new_entries;
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




void git_tree__free(git_tree *tree)
{
	size_t i;

	for (i = 0; i < tree->entry_count; ++i)
		free(tree->entries[i]);

	free(tree->entries);
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

	strncpy(entry->filename, name, GIT_TREE_MAX_FILENAME);
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
	return entry->filename;
}

const git_oid *git_tree_entry_id(git_tree_entry *entry)
{
	return &entry->oid;
}

git_object *git_tree_entry_2object(git_tree_entry *entry)
{
	return git_repository_lookup(entry->owner->object.repo, &entry->oid, GIT_OBJ_ANY);
}

git_tree_entry *git_tree_entry_byname(git_tree *tree, const char *filename)
{
	return *(git_tree_entry **)bsearch(filename, tree->entries, tree->entry_count, sizeof(git_tree_entry *), entry_cmp);
}

git_tree_entry *git_tree_entry_byindex(git_tree *tree, int idx)
{
	if (tree->entries == NULL)
		return NULL;

	return (idx >= 0 && idx < (int)tree->entry_count) ? tree->entries[idx] : NULL;
}

size_t git_tree_entrycount(git_tree *tree)
{
	return tree->entry_count;
}

void git_tree_add_entry(git_tree *tree, const git_oid *id, const char *filename, int attributes)
{
	git_tree_entry *entry;

	if (tree->entry_count >= tree->array_size)
		resize_tree_array(tree);

	if ((entry = git__malloc(sizeof(git_tree_entry))) == NULL)
		return;

	memset(entry, 0x0, sizeof(git_tree_entry));

	strncpy(entry->filename, filename, GIT_TREE_MAX_FILENAME);
	git_oid_cpy(&entry->oid, id);
	entry->attr = attributes;
	entry->owner = tree;

	tree->entries[tree->entry_count++] = entry;
	entry_resort(tree);

	tree->object.modified = 1;
}

int git_tree_remove_entry_byindex(git_tree *tree, int idx)
{
	git_tree_entry *remove_ptr;

	if (idx < 0 || idx >= (int)tree->entry_count)
		return GIT_ENOTFOUND;

	remove_ptr = tree->entries[idx];
	tree->entries[idx] = tree->entries[--tree->entry_count];

	free(remove_ptr);
	entry_resort(tree);

	tree->object.modified = 1;
	return GIT_SUCCESS;
}

int git_tree_remove_entry_byname(git_tree *tree, const char *filename)
{
	git_tree_entry **entry_ptr;
	int idx;

	entry_ptr = bsearch(filename, tree->entries, tree->entry_count, sizeof(git_tree_entry *), entry_cmp);
	if (entry_ptr == NULL)
		return GIT_ENOTFOUND;

	idx = (int)(entry_ptr - tree->entries);
	return git_tree_remove_entry_byindex(tree, idx);
}

int git_tree__writeback(git_tree *tree, git_odb_source *src)
{
	size_t i;

	if (tree->entries == NULL)
		return GIT_ERROR;

	entry_resort(tree);

	for (i = 0; i < tree->entry_count; ++i) {
		git_tree_entry *entry;
		entry = tree->entries[i];
	
		git__source_printf(src, "%06o %s\0", entry->attr, entry->filename);
		git__source_write(src, entry->oid.id, GIT_OID_RAWSZ);
	}

	return GIT_SUCCESS;
}


int git_tree__parse(git_tree *tree)
{
	static const size_t avg_entry_size = 40;

	int error = 0;
	char *buffer, *buffer_end;

	assert(!tree->object.in_memory);

	if (tree->entries != NULL) {
		size_t i;

		for (i = 0; i < tree->entry_count; ++i)
			free(tree->entries[i]);

		free(tree->entries);
	}

	error = git_object__source_open((git_object *)tree);
	if (error < 0)
		return error;

	buffer = tree->object.source.raw.data;
	buffer_end = buffer + tree->object.source.raw.len;

	tree->entry_count = 0;
	tree->array_size = (tree->object.source.raw.len / avg_entry_size) + 1;
	tree->entries = git__malloc(tree->array_size * sizeof(git_tree_entry *));

	while (buffer < buffer_end) {
		git_tree_entry *entry;

		if (tree->entry_count >= tree->array_size)
			resize_tree_array(tree);

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

		strncpy(entry->filename, buffer, GIT_TREE_MAX_FILENAME);

		while (buffer < buffer_end && *buffer != 0)
			buffer++;

		buffer++;

		git_oid_mkraw(&entry->oid, (const unsigned char *)buffer);
		buffer += GIT_OID_RAWSZ;
	}

	git_object__source_close((git_object *)tree);
	return error;
}
