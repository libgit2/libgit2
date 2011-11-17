/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "commit.h"
#include "tree.h"
#include "git2/repository.h"
#include "git2/object.h"

#define DEFAULT_TREE_SIZE 16
#define MAX_FILEMODE 0777777
#define MAX_FILEMODE_BYTES 6

#define ENTRY_IS_TREE(e) ((e)->attr & 040000)

static int valid_attributes(const int attributes)
{
	return attributes >= 0 && attributes <= MAX_FILEMODE;
}

static int valid_entry_name(const char *filename)
{
	return strlen(filename) > 0 && strchr(filename, '/') == NULL;
}

static int entry_sort_cmp(const void *a, const void *b)
{
	const git_tree_entry *entry_a = (const git_tree_entry *)(a);
	const git_tree_entry *entry_b = (const git_tree_entry *)(b);

	return git_futils_cmp_path(
		entry_a->filename, entry_a->filename_len, ENTRY_IS_TREE(entry_a),
		entry_b->filename, entry_b->filename_len, ENTRY_IS_TREE(entry_b));
}


struct tree_key_search {
	const char *filename;
	size_t filename_len;
};

static int homing_search_cmp(const void *key, const void *array_member)
{
	const struct tree_key_search *ksearch = key;
	const git_tree_entry *entry = array_member;

	const size_t len1 = ksearch->filename_len;
	const size_t len2 = entry->filename_len;

	return memcmp(
		ksearch->filename,
		entry->filename,
		len1 < len2 ? len1 : len2
	);
}

/*
 * Search for an entry in a given tree.
 *
 * Note that this search is performed in two steps because
 * of the way tree entries are sorted internally in git:
 *
 * Entries in a tree are not sorted alphabetically; two entries
 * with the same root prefix will have different positions
 * depending on whether they are folders (subtrees) or normal files.
 *
 * Consequently, it is not possible to find an entry on the tree
 * with a binary search if you don't know whether the filename
 * you're looking for is a folder or a normal file.
 *
 * To work around this, we first perform a homing binary search
 * on the tree, using the minimal length root prefix of our filename.
 * Once the comparisons for this homing search start becoming
 * ambiguous because of folder vs file sorting, we look linearly
 * around the area for our target file.
 */
static int tree_key_search(git_vector *entries, const char *filename)
{
	struct tree_key_search ksearch;
	const git_tree_entry *entry;

	int homing, i;

	ksearch.filename = filename;
	ksearch.filename_len = strlen(filename);

	/* Initial homing search; find an entry on the tree with
	 * the same prefix as the filename we're looking for */
	homing = git_vector_bsearch2(entries, &homing_search_cmp, &ksearch);
	if (homing < 0)
		return homing;

	/* We found a common prefix. Look forward as long as
	 * there are entries that share the common prefix */
	for (i = homing; i < (int)entries->length; ++i) {
		entry = entries->contents[i];

		if (homing_search_cmp(&ksearch, entry) != 0)
			break;

		if (strcmp(filename, entry->filename) == 0)
			return i;
	}

	/* If we haven't found our filename yet, look backwards
	 * too as long as we have entries with the same prefix */
	for (i = homing - 1; i >= 0; --i) {
		entry = entries->contents[i];

		if (homing_search_cmp(&ksearch, entry) != 0)
			break;

		if (strcmp(filename, entry->filename) == 0)
			return i;
	}

	/* The filename doesn't exist at all */
	return GIT_ENOTFOUND;
}

void git_tree__free(git_tree *tree)
{
	unsigned int i;

	for (i = 0; i < tree->entries.length; ++i) {
		git_tree_entry *e;
		e = git_vector_get(&tree->entries, i);

		git__free(e->filename);
		git__free(e);
	}

	git_vector_free(&tree->entries);
	git__free(tree);
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

	idx = tree_key_search(&tree->entries, filename);
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
		int tmp;

		entry = git__calloc(1, sizeof(git_tree_entry));
		if (entry == NULL) {
			error = GIT_ENOMEM;
			break;
		}

		if (git_vector_insert(&tree->entries, entry) < GIT_SUCCESS)
			return GIT_ENOMEM;

		if (git__strtol32(&tmp, buffer, &buffer, 8) < GIT_SUCCESS ||
			!buffer || !valid_attributes(tmp))
			return git__throw(GIT_EOBJCORRUPTED, "Failed to parse tree. Can't parse attributes");

		entry->attr = tmp;

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

static unsigned int find_next_dir(const char *dirname, git_index *index, unsigned int start)
{
	unsigned int i, entries = git_index_entrycount(index);
	size_t dirlen;

	dirlen = strlen(dirname);
	for (i = start; i < entries; ++i) {
		git_index_entry *entry = git_index_get(index, i);
		if (strlen(entry->path) < dirlen ||
		    memcmp(entry->path, dirname, dirlen) ||
			(dirlen > 0 && entry->path[dirlen] != '/')) {
			break;
		}
	}

	return i;
}

static int append_entry(git_treebuilder *bld, const char *filename, const git_oid *id, unsigned int attributes)
{
	git_tree_entry *entry;

	if ((entry = git__malloc(sizeof(git_tree_entry))) == NULL)
		return GIT_ENOMEM;

	memset(entry, 0x0, sizeof(git_tree_entry));
	entry->filename = git__strdup(filename);
	entry->filename_len = strlen(entry->filename);

	bld->entry_count++;

	git_oid_cpy(&entry->oid, id);
	entry->attr = attributes;

	if (git_vector_insert(&bld->entries, entry) < 0)
		return GIT_ENOMEM;

	return GIT_SUCCESS;
}

static int write_tree(git_oid *oid, git_index *index, const char *dirname, unsigned int start)
{
	git_treebuilder *bld = NULL;
	unsigned int i, entries = git_index_entrycount(index);
	int error;
	size_t dirname_len = strlen(dirname);
	const git_tree_cache *cache;

	cache = git_tree_cache_get(index->tree, dirname);
	if (cache != NULL && cache->entries >= 0){
		git_oid_cpy(oid, &cache->oid);
		return find_next_dir(dirname, index, start);
	}

	error = git_treebuilder_create(&bld, NULL);
	if (bld == NULL) {
		return GIT_ENOMEM;
	}

	/*
	 * This loop is unfortunate, but necessary. The index doesn't have
	 * any directores, so we need to handle that manually, and we
	 * need to keep track of the current position.
	 */
	for (i = start; i < entries; ++i) {
		git_index_entry *entry = git_index_get(index, i);
		char *filename, *next_slash;

	/*
	 * If we've left our (sub)tree, exit the loop and return. The
	 * first check is an early out (and security for the
	 * third). The second check is a simple prefix comparison. The
	 * third check catches situations where there is a directory
	 * win32/sys and a file win32mmap.c. Without it, the following
	 * code believes there is a file win32/mmap.c
	 */
		if (strlen(entry->path) < dirname_len ||
		    memcmp(entry->path, dirname, dirname_len) ||
		    (dirname_len > 0 && entry->path[dirname_len] != '/')) {
			break;
		}

		filename = entry->path + dirname_len;
		if (*filename == '/')
			filename++;
		next_slash = strchr(filename, '/');
		if (next_slash) {
			git_oid sub_oid;
			int written;
			char *subdir, *last_comp;

			subdir = git__strndup(entry->path, next_slash - entry->path);
			if (subdir == NULL) {
				error = GIT_ENOMEM;
				goto cleanup;
			}

			/* Write out the subtree */
			written = write_tree(&sub_oid, index, subdir, i);
			if (written < 0) {
				error = git__rethrow(written, "Failed to write subtree %s", subdir);
			} else {
				i = written - 1; /* -1 because of the loop increment */
			}

			/*
			 * We need to figure out what we want toinsert
			 * into this tree. If we're traversing
			 * deps/zlib/, then we only want to write
			 * 'zlib' into the tree.
			 */
			last_comp = strrchr(subdir, '/');
			if (last_comp) {
				last_comp++; /* Get rid of the '/' */
			} else {
				last_comp = subdir;
			}
			error = append_entry(bld, last_comp, &sub_oid, S_IFDIR);
			git__free(subdir);
			if (error < GIT_SUCCESS) {
				error = git__rethrow(error, "Failed to insert dir");
				goto cleanup;
			}
		} else {
			error = append_entry(bld, filename, &entry->oid, entry->mode);
			if (error < GIT_SUCCESS) {
				error = git__rethrow(error, "Failed to insert file");
			}
		}
	}

	error = git_treebuilder_write(oid, index->repository, bld);
	if (error < GIT_SUCCESS)
		error = git__rethrow(error, "Failed to write tree to db");

 cleanup:
	git_treebuilder_free(bld);

	if (error < GIT_SUCCESS)
		return error;
	else
		return i;
}

int git_tree_create_fromindex(git_oid *oid, git_index *index)
{
	int error;

	if (index->repository == NULL)
		return git__throw(GIT_EBAREINDEX, "Failed to create tree. The index file is not backed up by an existing repository");

	if (index->tree != NULL && index->tree->entries >= 0) {
		git_oid_cpy(oid, &index->tree->oid);
		return GIT_SUCCESS;
	}

	/* The tree cache didn't help us */
	error = write_tree(oid, index, "", 0);
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
		git__free(bld);
		return GIT_ENOMEM;
	}

	if (source != NULL) {
		for (i = 0; i < source->entries.length; ++i) {
			git_tree_entry *entry_src = source->entries.contents[i];

			if (append_entry(bld, entry_src->filename, &entry_src->oid, entry_src->attr) < 0) {
				git_treebuilder_free(bld);
				return GIT_ENOMEM;
			}
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
		return git__throw(GIT_ERROR, "Failed to insert entry. Invalid attributes");

	if (!valid_entry_name(filename))
		return git__throw(GIT_ERROR, "Failed to insert entry. Invalid name for a tree entry");

	pos = tree_key_search(&bld->entries, filename);

	if (pos >= 0) {
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

static git_tree_entry *treebuilder_get(git_treebuilder *bld, const char *filename)
{
	int idx;
	git_tree_entry *entry;

	assert(bld && filename);

	idx = tree_key_search(&bld->entries, filename);
	if (idx < 0)
		return NULL;

	entry = git_vector_get(&bld->entries, idx);
	if (entry->removed)
		return NULL;

	return entry;
}

const git_tree_entry *git_treebuilder_get(git_treebuilder *bld, const char *filename)
{
	return treebuilder_get(bld, filename);
}

int git_treebuilder_remove(git_treebuilder *bld, const char *filename)
{
	git_tree_entry *remove_ptr = treebuilder_get(bld, filename);

	if (remove_ptr == NULL || remove_ptr->removed)
		return git__throw(GIT_ENOTFOUND, "Failed to remove entry. File isn't in the tree");

	remove_ptr->removed = 1;
	bld->entry_count--;
	return GIT_SUCCESS;
}

int git_treebuilder_write(git_oid *oid, git_repository *repo, git_treebuilder *bld)
{
	unsigned int i;
	int error;
	git_buf tree = GIT_BUF_INIT;

	assert(bld);

	sort_entries(bld);

	/* Grow the buffer beforehand to an estimated size */
	git_buf_grow(&tree, bld->entries.length * 72);

	for (i = 0; i < bld->entries.length; ++i) {
		git_tree_entry *entry = bld->entries.contents[i];

		if (entry->removed)
			continue;

		git_buf_printf(&tree, "%o ", entry->attr);
		git_buf_put(&tree, entry->filename, entry->filename_len + 1);
		git_buf_put(&tree, (char *)entry->oid.id, GIT_OID_RAWSZ);
	}

	if (git_buf_oom(&tree)) {
		git_buf_free(&tree);
		return git__throw(GIT_ENOMEM, "Not enough memory to build the tree data");
	}

	error = git_odb_write(oid, git_repository_database(repo), tree.ptr, tree.size, GIT_OBJ_TREE);
	git_buf_free(&tree);

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
		git__free(e->filename);
		git__free(e);
	}

	git_vector_clear(&bld->entries);
}

void git_treebuilder_free(git_treebuilder *bld)
{
	git_treebuilder_clear(bld);
	git_vector_free(&bld->entries);
	git__free(bld);
}

static int tree_frompath(
	git_tree **parent_out,
	git_tree *root,
	const char *treeentry_path,
	int offset)
{
	char *slash_pos = NULL;
	const git_tree_entry* entry;
	int error = GIT_SUCCESS;
	git_tree *subtree;

	if (!*(treeentry_path + offset))
		return git__rethrow(GIT_EINVALIDPATH,
			"Invalid relative path to a tree entry '%s'.", treeentry_path);

	slash_pos = (char *)strchr(treeentry_path + offset, '/');

	if (slash_pos == NULL)
		return git_tree_lookup(
			parent_out,
			root->object.repo,
			git_object_id((const git_object *)root)
		);

	if (slash_pos == treeentry_path + offset)
		return git__rethrow(GIT_EINVALIDPATH,
			"Invalid relative path to a tree entry '%s'.", treeentry_path);

	*slash_pos = '\0';

	entry = git_tree_entry_byname(root, treeentry_path + offset);

	if (slash_pos != NULL)
		*slash_pos = '/';

	if (entry == NULL)
		return git__rethrow(GIT_ENOTFOUND,
			"No tree entry can be found from "
			"the given tree and relative path '%s'.", treeentry_path);


	error = git_tree_lookup(&subtree, root->object.repo, &entry->oid);
	if (error < GIT_SUCCESS)
		return error;

	error = tree_frompath(
		parent_out,
		subtree,
		treeentry_path,
		slash_pos - treeentry_path + 1
	);

	git_tree_close(subtree);
	return error;
}

int git_tree_get_subtree(
	git_tree **subtree,
	git_tree *root,
	const char *subtree_path)
{
	char buffer[GIT_PATH_MAX];

	assert(subtree && root && subtree_path);

	strncpy(buffer, subtree_path, GIT_PATH_MAX);
	return tree_frompath(subtree, root, buffer, 0);
}

static int tree_walk_post(
	git_tree *tree,
	git_treewalk_cb callback,
	char *root,
	size_t root_len,
	void *payload)
{
	int error;
	unsigned int i;

	for (i = 0; i < tree->entries.length; ++i) {
		git_tree_entry *entry = tree->entries.contents[i];

		root[root_len] = '\0';

		if (callback(root, entry, payload) < 0)
			continue;

		if (ENTRY_IS_TREE(entry)) {
			git_tree *subtree;

			if ((error = git_tree_lookup(
				&subtree, tree->object.repo, &entry->oid)) < 0)
				return error;

			strcpy(root + root_len, entry->filename);
			root[root_len + entry->filename_len] = '/';

			tree_walk_post(subtree,
				callback, root,
				root_len + entry->filename_len + 1,
				payload
			);

			git_tree_close(subtree);
		}
	}

	return GIT_SUCCESS;
}

int git_tree_walk(git_tree *tree, git_treewalk_cb callback, int mode, void *payload)
{
	char root_path[GIT_PATH_MAX];

	root_path[0] = '\0';
	switch (mode) {
		case GIT_TREEWALK_POST:
			return tree_walk_post(tree, callback, root_path, 0, payload);

		case GIT_TREEWALK_PRE:
			return git__throw(GIT_ENOTIMPLEMENTED,
				"Preorder tree walking is still not implemented");

		default:
			return git__throw(GIT_EINVALIDARGS,
				"Invalid walking mode for tree walk");
	}
}
