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

void git_tree__free(git_tree *tree)
{
	size_t i;

	for (i = 0; i < tree->entry_count; ++i)
		free(tree->entries[i].filename);

	free(tree->entries);
	free(tree);
}

const git_oid *git_tree_id(git_tree *tree)
{
	return &tree->object.id;
}

git_tree *git_tree_lookup(git_repository *repo, const git_oid *id)
{
	return (git_tree *)git_repository_lookup(repo, id, GIT_OBJ_TREE);
}

uint32_t git_tree_entry_attributes(const git_tree_entry *entry)
{
	return entry->attr;
}

const char *git_tree_entry_name(const git_tree_entry *entry)
{
	return entry->filename;
}

const git_oid *git_tree_entry_id(const git_tree_entry *entry)
{
	return &entry->oid;
}

git_repository_object *git_tree_entry_2object(const git_tree_entry *entry)
{
	return git_repository_lookup(entry->owner->object.repo, &entry->oid, GIT_OBJ_ANY);
}

int entry_cmp(const void *key, const void *array_member)
{
	const char *filename = (const char *)key;
	const git_tree_entry *entry = (const git_tree_entry *)array_member;

	return strcmp(filename, entry->filename);
}

const git_tree_entry *git_tree_entry_byname(git_tree *tree, const char *filename)
{
	if (tree->entries == NULL)
		git_tree__parse(tree);

	return bsearch(filename, tree->entries, tree->entry_count, sizeof(git_tree_entry), entry_cmp);
}

const git_tree_entry *git_tree_entry_byindex(git_tree *tree, int idx)
{
	if (tree->entries == NULL)
		git_tree__parse(tree);

	return (tree->entries && idx >= 0 && idx < (int)tree->entry_count) ? 
		&tree->entries[idx] : NULL;
}

size_t git_tree_entrycount(git_tree *tree)
{
	return tree->entry_count;
}

int git_tree__parse(git_tree *tree)
{
	static const size_t avg_entry_size = 40;

	int error = 0;
	git_obj odb_object;
	char *buffer, *buffer_end;
	size_t entries_size;

	if (tree->entries != NULL)
		return GIT_SUCCESS;

	error = git_odb_read(&odb_object, tree->object.repo->db, &tree->object.id);
	if (error < 0)
		return error;

	buffer = odb_object.data;
	buffer_end = odb_object.data + odb_object.len;

	tree->entry_count = 0;
	entries_size = (odb_object.len / avg_entry_size) + 1;
	tree->entries = git__malloc(entries_size * sizeof(git_tree_entry));

	while (buffer < buffer_end) {
		git_tree_entry *entry;

		if (tree->entry_count >= entries_size) {
			git_tree_entry *new_entries;

			entries_size = entries_size * 2;

			new_entries = git__malloc(entries_size * sizeof(git_tree_entry));
			memcpy(new_entries, tree->entries, tree->entry_count * sizeof(git_tree_entry));

			free(tree->entries);
			tree->entries = new_entries;
		}

		entry = &tree->entries[tree->entry_count++];
		entry->owner = tree;

		entry->attr = strtol(buffer, &buffer, 8);

		if (*buffer++ != ' ') {
			error = GIT_EOBJCORRUPTED;
			break;
		}

		entry->filename = git__strdup(buffer);

		if (entry->filename == NULL) {
			error = GIT_EOBJCORRUPTED;
		}

		buffer += strlen(entry->filename) + 1;

		git_oid_mkraw(&entry->oid, (const unsigned char *)buffer);
		buffer += GIT_OID_RAWSZ;
	}

	git_obj_close(&odb_object);
	return error;
}
