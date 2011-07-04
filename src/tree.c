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

static int valid_attributes(const int attributes) {
	return attributes >= 0 && attributes <= MAX_FILEMODE;
}

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

	return git_futils_cmp_path(entry_a->filename, strlen(entry_a->filename),
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

unsigned int git_tree_entry_attributes(const git_tree_entry *entry)
{
	return entry->attr;
}

const char *git_tree_entry_name(const git_tree_entry *entry)
{
	assert(entry);
	return entry->filename;
}

const git_oid *git_tree_entry_id(const git_tree_entry *entry)
{
	assert(entry);
	return &entry->oid;
}

git_otype git_tree_entry_type(const git_tree_entry *entry)
{
	assert(entry);

	if (S_ISGITLINK(entry->attr))
		return GIT_OBJ_COMMIT;
	else if (S_ISDIR(entry->attr))
		return GIT_OBJ_TREE;
	else
		return GIT_OBJ_BLOB;
}

int git_tree_entry_2object(git_object **object_out, git_repository *repo, const git_tree_entry *entry)
{
	assert(entry && object_out);
	return git_object_lookup(object_out, repo, &entry->oid, GIT_OBJ_ANY);
}

const git_tree_entry *git_tree_entry_byname(git_tree *tree, const char *filename)
{
	int idx;

	assert(tree && filename);

	idx = git_vector_bsearch2(&tree->entries, entry_search_cmp, filename);
	if (idx == GIT_ENOTFOUND)
		return NULL;

	return git_vector_get(&tree->entries, idx);
}

const git_tree_entry *git_tree_entry_byindex(git_tree *tree, unsigned int idx)
{
	assert(tree);
	return git_vector_get(&tree->entries, idx);
}

unsigned int git_tree_entrycount(git_tree *tree)
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

		if (git__strtol32((long *)&entry->attr, buffer, &buffer, 8) < GIT_SUCCESS)
			return git__throw(GIT_EOBJCORRUPTED, "Failed to parse tree. Can't parse attributes");

		if (*buffer++ != ' ') {
			error = git__throw(GIT_EOBJCORRUPTED, "Failed to parse tree. Object it corrupted");
			break;
		}

		if (memchr(buffer, 0, buffer_end - buffer) == NULL) {
			error = git__throw(GIT_EOBJCORRUPTED, "Failed to parse tree. Object it corrupted");
			break;
		}

		entry->filename = git__strdup(buffer);
		entry->filename_len = strlen(buffer);

		while (buffer < buffer_end && *buffer != 0)
			buffer++;

		buffer++;

		git_oid_fromraw(&entry->oid, (const unsigned char *)buffer);
		buffer += GIT_OID_RAWSZ;
	}

	return error == GIT_SUCCESS ? GIT_SUCCESS : git__rethrow(error, "Failed to parse buffer");
}

int git_tree__parse(git_tree *tree, git_odb_object *obj)
{
	assert(tree);
	return tree_parse_buffer(tree, (char *)obj->raw.data, (char *)obj->raw.data + obj->raw.len);
}

static int write_index_entry(char *buffer, int mode, const char *path, size_t path_len, const git_oid *oid)
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

		offset += write_index_entry(buffer + offset, write_mode, filename, entrylen, write_oid);
	}

	error = git_odb_write(oid, index->repository->db, buffer, offset, GIT_OBJ_TREE);
	free(buffer);

	return (error == GIT_SUCCESS) ? nr : git__rethrow(error, "Failed to write index");
}

int git_tree_create_fromindex(git_oid *oid, git_index *index)
{
	int error;

	if (index->repository == NULL)
		return git__throw(GIT_EBAREINDEX, "Failed to create tree. The index file is not backed up by an existing repository");

	error = write_index(oid, index, "", 0, 0, git_index_entrycount(index));
	return (error < GIT_SUCCESS) ? git__rethrow(error, "Failed to create tree") : GIT_SUCCESS;
}

static void sort_entries(git_treebuilder *bld)
{
	git_vector_sort(&bld->entries);
}

int git_treebuilder_create(git_treebuilder **builder_p, const git_tree *source)
{
	git_treebuilder *bld;
	unsigned int i, source_entries = DEFAULT_TREE_SIZE;

	assert(builder_p);

	bld = git__calloc(1, sizeof(git_treebuilder));
	if (bld == NULL)
		return GIT_ENOMEM;

	if (source != NULL)
		source_entries = source->entries.length;

	if (git_vector_init(&bld->entries, source_entries, entry_sort_cmp) < GIT_SUCCESS) {
		free(bld);
		return GIT_ENOMEM;
	}

	if (source != NULL) {
		bld->entry_count = source_entries;
		for (i = 0; i < source->entries.length; ++i) {
			git_tree_entry *entry_src = source->entries.contents[i];
			git_tree_entry *entry = git__calloc(1, sizeof(git_tree_entry));

			if (entry == NULL) {
				git_treebuilder_free(bld);
				return GIT_ENOMEM;
			}

			entry->filename = git__strdup(entry_src->filename);

			if (entry->filename == NULL) {
				free(entry);
				git_treebuilder_free(bld);
				return GIT_ENOMEM;
			}

			entry->filename_len = entry_src->filename_len;
			git_oid_cpy(&entry->oid, &entry_src->oid);
			entry->attr = entry_src->attr;

			git_vector_insert(&bld->entries, entry);
		}
	}

	*builder_p = bld;
	return GIT_SUCCESS;
}

int git_treebuilder_insert(git_tree_entry **entry_out, git_treebuilder *bld, const char *filename, const git_oid *id, unsigned int attributes)
{
	git_tree_entry *entry;
	int pos;

	assert(bld && id && filename);

	if (!valid_attributes(attributes))
		return git__throw(GIT_ERROR, "Failed to insert entry. Invalid atrributes");

	if ((pos = git_vector_bsearch2(&bld->entries, entry_search_cmp, filename)) != GIT_ENOTFOUND) {
		entry = git_vector_get(&bld->entries, pos);
		if (entry->removed) {
			entry->removed = 0;
			bld->entry_count++;
		}
	} else {
		if ((entry = git__malloc(sizeof(git_tree_entry))) == NULL)
			return GIT_ENOMEM;

		memset(entry, 0x0, sizeof(git_tree_entry));
		entry->filename = git__strdup(filename);
		entry->filename_len = strlen(entry->filename);

		bld->entry_count++;
	}

	git_oid_cpy(&entry->oid, id);
	entry->attr = attributes;

	if (pos == GIT_ENOTFOUND) {
		if (git_vector_insert(&bld->entries, entry) < 0)
			return GIT_ENOMEM;
	}

	if (entry_out != NULL)
		*entry_out = entry;

	return GIT_SUCCESS;
}

const git_tree_entry *git_treebuilder_get(git_treebuilder *bld, const char *filename)
{
	int idx;
	git_tree_entry *entry;

	assert(bld && filename);

	sort_entries(bld);
	idx = git_vector_bsearch2(&bld->entries, entry_search_cmp, filename);
	if (idx == GIT_ENOTFOUND)
		return NULL;

	entry = git_vector_get(&bld->entries, idx);
	if (entry->removed)
		return NULL;

	return entry;
}

int git_treebuilder_remove(git_treebuilder *bld, const char *filename)
{
	git_tree_entry *remove_ptr = (git_tree_entry *)git_treebuilder_get(bld, filename);

	if (remove_ptr == NULL || remove_ptr->removed)
		return git__throw(GIT_ENOTFOUND, "Failed to remove entry. File isn't in the tree");

	remove_ptr->removed = 1;
	bld->entry_count--;
	return GIT_SUCCESS;
}

int git_treebuilder_write(git_oid *oid, git_repository *repo, git_treebuilder *bld)
{
	unsigned int i, size = 0;
	char filemode[MAX_FILEMODE_BYTES + 1 + 1];
	git_odb_stream *stream;
	int error;

	assert(bld);

	sort_entries(bld);

	for (i = 0; i < bld->entries.length; ++i) {
		git_tree_entry *entry = bld->entries.contents[i];

		if (entry->removed)
			continue;

		snprintf(filemode, sizeof(filemode), "%o ", entry->attr);
		size += strlen(filemode);
		size += entry->filename_len + 1;
		size += GIT_OID_RAWSZ;
	}

	if ((error = git_odb_open_wstream(&stream, git_repository_database(repo), size, GIT_OBJ_TREE)) < GIT_SUCCESS)
		return git__rethrow(error, "Failed to write tree. Can't open write stream");

	for (i = 0; i < bld->entries.length; ++i) {
		git_tree_entry *entry = bld->entries.contents[i];

		if (entry->removed)
			continue;

		snprintf(filemode, sizeof(filemode), "%o ", entry->attr);
		stream->write(stream, filemode, strlen(filemode));
		stream->write(stream, entry->filename, entry->filename_len + 1);
		stream->write(stream, (char *)entry->oid.id, GIT_OID_RAWSZ);
	}

	error = stream->finalize_write(oid, stream);
	stream->free(stream);

	return error == GIT_SUCCESS ? GIT_SUCCESS : git__rethrow(error, "Failed to write tree");
}

void git_treebuilder_filter(git_treebuilder *bld, int (*filter)(const git_tree_entry *, void *), void *payload)
{
	unsigned int i;

	assert(bld && filter);

	for (i = 0; i < bld->entries.length; ++i) {
		git_tree_entry *entry = bld->entries.contents[i];
		if (!entry->removed && filter(entry, payload))
			entry->removed = 1;
	}
}

void git_treebuilder_clear(git_treebuilder *bld)
{
	unsigned int i;
	assert(bld);

	for (i = 0; i < bld->entries.length; ++i) {
		git_tree_entry *e = bld->entries.contents[i];
		free(e->filename);
		free(e);
	}

	git_vector_clear(&bld->entries);
}

void git_treebuilder_free(git_treebuilder *bld)
{
	git_treebuilder_clear(bld);
	git_vector_free(&bld->entries);
	free(bld);
}


