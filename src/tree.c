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

static int tree_parse_buffer(git_tree *tree, const char *buffer, const char *buffer_end)
{
	int error = GIT_SUCCESS;

	if (git_vector_init(&tree->entries, DEFAULT_TREE_SIZE, entry_sort_cmp) < GIT_SUCCESS)
		return GIT_ENOMEM;

	while (buffer < buffer_end) {
		git_tree_entry *entry;

		entry = git__calloc(1, sizeof(git_tree_entry));
		if (entry == NULL) {
			error = GIT_ENOMEM;
			break;
		}

		if (git_vector_insert(&tree->entries, entry) < GIT_SUCCESS)
			return GIT_ENOMEM;

		entry->attr = strtol(buffer, (char **)&buffer, 8);

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

static int write_entry(char *buffer, int mode, const char *path, size_t path_len, const git_oid *oid)
{
	int written;
	written = sprintf(buffer, "%o %.*s%c", mode, (int)path_len, path, 0);
	memcpy(buffer + written, &oid->id, GIT_OID_RAWSZ);
	return written + GIT_OID_RAWSZ;
}

static int write_index(git_oid *oid, git_index *index, const char *base, int baselen, int entry_no, int maxentries)
{
	size_t size, offset;
	char *buffer;
	int nr, error;

	/* Guess at some random initial size */
	size = maxentries * 40;
	buffer = git__malloc(size);
	if (buffer == NULL)
		return GIT_ENOMEM;
		
	offset = 0;
	
	for (nr = entry_no; nr < maxentries; ++nr) {
		git_index_entry *entry = git_index_get(index, nr);

		const char *pathname = entry->path, *filename, *dirname;
		int pathlen = strlen(pathname), entrylen;

		unsigned int write_mode;
		git_oid subtree_oid;
		git_oid *write_oid;
		
		/* Did we hit the end of the directory? Return how many we wrote */
		if (baselen >= pathlen || memcmp(base, pathname, baselen) != 0)
			break;
		
		/* Do we have _further_ subdirectories? */
		filename = pathname + baselen;
		dirname = strchr(filename, '/');

		write_oid = &entry->oid;
		write_mode = entry->mode;

		if (dirname) {
			int subdir_written;

#if 0
			if (entry->mode != S_IFDIR) {
				free(buffer);
				return GIT_EOBJCORRUPTED;
			}
#endif
			subdir_written = write_index(&subtree_oid, index, pathname, dirname - pathname + 1, nr, maxentries);

			if (subdir_written < GIT_SUCCESS) {
				free(buffer);
				return subdir_written;
			}
			
			nr = subdir_written - 1;
			
			/* Now we need to write out the directory entry into this tree.. */
			pathlen = dirname - pathname;
			write_oid = &subtree_oid;
			write_mode = S_IFDIR;
		}

		entrylen = pathlen - baselen;
		if (offset + entrylen + 32 > size) {
			size = alloc_nr(offset + entrylen + 32);
			buffer = git__realloc(buffer, size);
			
			if (buffer == NULL)
				return GIT_ENOMEM;
		}

		offset += write_entry(buffer + offset, write_mode, filename, entrylen, write_oid);
	}
	
	error = git_odb_write(oid, index->repository->db, buffer, offset, GIT_OBJ_TREE);
	free(buffer);

	return (error == GIT_SUCCESS) ? nr : error;
}

int git_tree_create_fromindex(git_oid *oid, git_index *index)
{
	int error;

	if (index->repository == NULL)
		return GIT_EBAREINDEX;

	error = write_index(oid, index, "", 0, 0, git_index_entrycount(index));
	return (error < GIT_SUCCESS) ? error : GIT_SUCCESS;
}
