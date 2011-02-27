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
#include "git2/repository.h"
#include "git2/object.h"

#define DEFAULT_TREE_SIZE 16

int entry_search_cmp(const void *key, const void *array_member)
{
	const char *filename = (const char *)key;
	const git_tree_entry *entry = *(const git_tree_entry **)(array_member);

	return strcmp(filename, entry->filename);
}

int entry_sort_cmp(const void *a, const void *b)
{
	const git_tree_entry *entry_a = *(const git_tree_entry **)(a);
	const git_tree_entry *entry_b = *(const git_tree_entry **)(b);

	return gitfo_cmp_path(entry_a->filename, strlen(entry_a->filename),
                                  entry_a->attr & 040000,
                                  entry_b->filename, strlen(entry_b->filename),
                                  entry_b->attr & 040000);
}

void git_tree_clear_entries(git_tree *tree)
{
	unsigned int i;

	if (tree == NULL)
		return;

	for (i = 0; i < tree->entries.length; ++i) {
		git_tree_entry *e;
		e = git_vector_get(&tree->entries, i);

		free(e->filename);
		free(e);
	}

	git_vector_clear(&tree->entries);

	tree->object.modified = 1;
	tree->sorted = 1;
}


git_tree *git_tree__new(void)
{
	git_tree *tree;

	tree = git__malloc(sizeof(struct git_tree));
	if (tree == NULL)
		return NULL;

	memset(tree, 0x0, sizeof(struct git_tree));

	if (git_vector_init(&tree->entries, 
				DEFAULT_TREE_SIZE,
				entry_sort_cmp,
				entry_search_cmp) < GIT_SUCCESS) {
		free(tree);
		return NULL;
	}

	tree->sorted = 1;
	return tree;
}

void git_tree__free(git_tree *tree)
{
	git_tree_clear_entries(tree);
	git_vector_free(&tree->entries);
	free(tree);
}

const git_oid *git_tree_id(git_tree *c)
{
	return git_object_id((git_object *)c);
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
	git_vector_sort(&entry->owner->entries);
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

int git_tree_entry_2object(git_object **object_out, git_tree_entry *entry)
{
	assert(entry && object_out);
	return git_repository_lookup(object_out, entry->owner->object.repo, &entry->oid, GIT_OBJ_ANY);
}

static void sort_entries(git_tree *tree)
{
	if (tree->sorted == 0) {
        git_vector_sort(&tree->entries);
		tree->sorted = 1;
	}
}

git_tree_entry *git_tree_entry_byname(git_tree *tree, const char *filename)
{
	int idx;

	assert(tree && filename);

	if (!tree->sorted)
		sort_entries(tree);

	idx = git_vector_search(&tree->entries, filename);
	if (idx == GIT_ENOTFOUND)
		return NULL;

	return git_vector_get(&tree->entries, idx);
}

git_tree_entry *git_tree_entry_byindex(git_tree *tree, int idx)
{
	assert(tree);

	if (!tree->sorted)
		sort_entries(tree);

	return git_vector_get(&tree->entries, (unsigned int)idx);
}

size_t git_tree_entrycount(git_tree *tree)
{
	assert(tree);
	return tree->entries.length;
}

int git_tree_add_entry(git_tree_entry **entry_out, git_tree *tree, const git_oid *id, const char *filename, int attributes)
{
	git_tree_entry *entry;

	assert(tree && id && filename);

	if ((entry = git__malloc(sizeof(git_tree_entry))) == NULL)
		return GIT_ENOMEM;

	memset(entry, 0x0, sizeof(git_tree_entry));

	entry->filename = git__strdup(filename);
	git_oid_cpy(&entry->oid, id);
	entry->attr = attributes;
	entry->owner = tree;

	if (git_vector_insert(&tree->entries, entry) < 0)
		return GIT_ENOMEM;

	if (entry_out != NULL)
		*entry_out = entry;

	tree->object.modified = 1;
	tree->sorted = 0;
	return GIT_SUCCESS;
}

int git_tree_remove_entry_byindex(git_tree *tree, int idx)
{
	git_tree_entry *remove_ptr;

	assert(tree);

	if (!tree->sorted)
		sort_entries(tree);

	remove_ptr = git_vector_get(&tree->entries, (unsigned int)idx);
	if (remove_ptr == NULL)
		return GIT_ENOTFOUND;

	free(remove_ptr->filename);
	free(remove_ptr);

	tree->object.modified = 1;

	return git_vector_remove(&tree->entries, (unsigned int)idx);
}

int git_tree_remove_entry_byname(git_tree *tree, const char *filename)
{
	int idx;

	assert(tree && filename);

	if (!tree->sorted)
		sort_entries(tree);

	idx = git_vector_search(&tree->entries, filename);
	if (idx == GIT_ENOTFOUND)
		return GIT_ENOTFOUND;

	return git_tree_remove_entry_byindex(tree, idx);
}

int git_tree__writeback(git_tree *tree, git_odb_source *src)
{
	size_t i;
	char filemode[8];

	assert(tree && src);

	if (tree->entries.length == 0)
		return GIT_EMISSINGOBJDATA;

	if (!tree->sorted)
		sort_entries(tree);

	for (i = 0; i < tree->entries.length; ++i) {
		git_tree_entry *entry;

		entry = git_vector_get(&tree->entries, i);
	
		sprintf(filemode, "%o ", entry->attr);

		git__source_write(src, filemode, strlen(filemode));
		git__source_write(src, entry->filename, strlen(entry->filename) + 1);
		git__source_write(src, entry->oid.id, GIT_OID_RAWSZ);
	} 

	return GIT_SUCCESS;
}


static int tree_parse_buffer(git_tree *tree, char *buffer, char *buffer_end)
{
	static const size_t avg_entry_size = 40;
	unsigned int expected_size;
	int error = GIT_SUCCESS;

	expected_size = (tree->object.source.raw.len / avg_entry_size) + 1;

	git_tree_clear_entries(tree);

	while (buffer < buffer_end) {
		git_tree_entry *entry;

		entry = git__malloc(sizeof(git_tree_entry));
		if (entry == NULL) {
			error = GIT_ENOMEM;
			break;
		}

		if (git_vector_insert(&tree->entries, entry) < GIT_SUCCESS)
			return GIT_ENOMEM;

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

