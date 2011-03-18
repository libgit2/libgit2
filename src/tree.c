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
#include "tree.h"
#include "git2/repository.h"
#include "git2/object.h"

#define DEFAULT_TREE_SIZE 16
#define MAX_FILEMODE 0777777
#define MAX_FILEMODE_BYTES 6

int entry_search_cmp(const void *key, const void *array_member)
{
	const char *filename = (const char *)key;
	const git_tree_entry *entry = *(const git_tree_entry **)(array_member);

	return strcmp(filename, entry->filename);
}

#if 0
static int valid_attributes(const int attributes) {
	return attributes >= 0 && attributes <= MAX_FILEMODE; 
}
#endif

int entry_sort_cmp(const void *a, const void *b)
{
	const git_tree_entry *entry_a = *(const git_tree_entry **)(a);
	const git_tree_entry *entry_b = *(const git_tree_entry **)(b);

	return gitfo_cmp_path(entry_a->filename, strlen(entry_a->filename),
                                  entry_a->attr & 040000,
                                  entry_b->filename, strlen(entry_b->filename),
                                  entry_b->attr & 040000);
}

void git_tree__free(git_tree *tree)
{
	unsigned int i;

	for (i = 0; i < tree->entries.length; ++i) {
		git_tree_entry *e;
		e = git_vector_get(&tree->entries, i);

		free(e->filename);
		free(e);
	}

	git_vector_free(&tree->entries);
	free(tree);
}

const git_oid *git_tree_id(git_tree *c)
{
	return git_object_id((git_object *)c);
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

int git_tree_entry_2object(git_object **object_out, git_repository *repo, git_tree_entry *entry)
{
	assert(entry && object_out);
	return git_object_lookup(object_out, repo, &entry->oid, GIT_OBJ_ANY);
}

git_tree_entry *git_tree_entry_byname(git_tree *tree, const char *filename)
{
	int idx;

	assert(tree && filename);

	idx = git_vector_bsearch2(&tree->entries, entry_search_cmp, filename);
	if (idx == GIT_ENOTFOUND)
		return NULL;

	return git_vector_get(&tree->entries, idx);
}

git_tree_entry *git_tree_entry_byindex(git_tree *tree, int idx)
{
	assert(tree);
	return git_vector_get(&tree->entries, (unsigned int)idx);
}

size_t git_tree_entrycount(git_tree *tree)
{
	assert(tree);
	return tree->entries.length;
}

static int tree_parse_buffer(git_tree *tree, char *buffer, char *buffer_end)
{
	int error = GIT_SUCCESS;

	if (git_vector_init(&tree->entries, DEFAULT_TREE_SIZE, entry_sort_cmp) < GIT_SUCCESS)
		return GIT_ENOMEM;

	while (buffer < buffer_end) {
		git_tree_entry *entry;

		entry = git__malloc(sizeof(git_tree_entry));
		if (entry == NULL) {
			error = GIT_ENOMEM;
			break;
		}

		if (git_vector_insert(&tree->entries, entry) < GIT_SUCCESS)
			return GIT_ENOMEM;

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

int git_tree__parse(git_tree *tree, git_odb_object *obj)
{
	assert(tree);
	return tree_parse_buffer(tree, (char *)obj->raw.data, (char *)obj->raw.data + obj->raw.len);
}

