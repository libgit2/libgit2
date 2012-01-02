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
		entry_a->filename, entry_a->filename_len, entry_is_tree(entry_a),
		entry_b->filename, entry_b->filename_len, entry_is_tree(entry_b));
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

static int write_tree(
	git_oid *oid,
	git_repository *repo,
	git_index *index,
	const char *dirname,
	unsigned int start)
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
			written = write_tree(&sub_oid, repo, index, subdir, i);
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

	error = git_treebuilder_write(oid, repo, bld);
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
	git_repository *repo;
	int error;

	repo = (git_repository *)GIT_REFCOUNT_OWNER(index);

	if (repo == NULL)
		return git__throw(GIT_EBAREINDEX,
			"Failed to create tree. "
			"The index file is not backed up by an existing repository");

	if (index->tree != NULL && index->tree->entries >= 0) {
		git_oid_cpy(oid, &index->tree->oid);
		return GIT_SUCCESS;
	}

	/* The tree cache didn't help us */
	error = write_tree(oid, repo, index, "", 0);
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
	git_odb *odb;

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

	if ((error = git_buf_lasterror(&tree)) < GIT_SUCCESS) {
		git_buf_free(&tree);
		return git__rethrow(error, "Not enough memory to build the tree data");
	}

	error = git_repository_odb__weakptr(&odb, repo);
	if (error < GIT_SUCCESS) {
		git_buf_free(&tree);
		return error;
	}

	error = git_odb_write(oid, odb, tree.ptr, tree.size, GIT_OBJ_TREE);
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
	git_buf *treeentry_path,
	int offset)
{
	char *slash_pos = NULL;
	const git_tree_entry* entry;
	int error = GIT_SUCCESS;
	git_tree *subtree;

	if (!*(treeentry_path->ptr + offset))
		return git__rethrow(GIT_EINVALIDPATH,
			"Invalid relative path to a tree entry '%s'.", treeentry_path->ptr);

	slash_pos = (char *)strchr(treeentry_path->ptr + offset, '/');

	if (slash_pos == NULL)
		return git_tree_lookup(
			parent_out,
			root->object.repo,
			git_object_id((const git_object *)root)
		);

	if (slash_pos == treeentry_path->ptr + offset)
		return git__rethrow(GIT_EINVALIDPATH,
			"Invalid relative path to a tree entry '%s'.", treeentry_path->ptr);

	*slash_pos = '\0';

	entry = git_tree_entry_byname(root, treeentry_path->ptr + offset);

	if (slash_pos != NULL)
		*slash_pos = '/';

	if (entry == NULL)
		return git__rethrow(GIT_ENOTFOUND,
			"No tree entry can be found from "
			"the given tree and relative path '%s'.", treeentry_path->ptr);


	error = git_tree_lookup(&subtree, root->object.repo, &entry->oid);
	if (error < GIT_SUCCESS)
		return error;

	error = tree_frompath(
		parent_out,
		subtree,
		treeentry_path,
		(slash_pos - treeentry_path->ptr) + 1
	);

	git_tree_free(subtree);
	return error;
}

int git_tree_get_subtree(
	git_tree **subtree,
	git_tree *root,
	const char *subtree_path)
{
	int error;
	git_buf buffer = GIT_BUF_INIT;

	assert(subtree && root && subtree_path);

	if ((error = git_buf_sets(&buffer, subtree_path)) == GIT_SUCCESS)
		error = tree_frompath(subtree, root, &buffer, 0);

	git_buf_free(&buffer);

	return error;
}

static int tree_walk_post(
	git_tree *tree,
	git_treewalk_cb callback,
	git_buf *path,
	void *payload)
{
	int error = GIT_SUCCESS;
	unsigned int i;

	for (i = 0; i < tree->entries.length; ++i) {
		git_tree_entry *entry = tree->entries.contents[i];

		if (callback(path->ptr, entry, payload) < 0)
			continue;

		if (entry_is_tree(entry)) {
			git_tree *subtree;
			size_t path_len = path->size;

			if ((error = git_tree_lookup(
				&subtree, tree->object.repo, &entry->oid)) < 0)
				break;

			/* append the next entry to the path */
			git_buf_puts(path, entry->filename);
			git_buf_putc(path, '/');
			if ((error = git_buf_lasterror(path)) < GIT_SUCCESS)
				break;

			error = tree_walk_post(subtree, callback, path, payload);
			if (error < GIT_SUCCESS)
				break;

			git_buf_truncate(path, path_len);
			git_tree_free(subtree);
		}
	}

	return error;
}

int git_tree_walk(git_tree *tree, git_treewalk_cb callback, int mode, void *payload)
{
	int error = GIT_SUCCESS;
	git_buf root_path = GIT_BUF_INIT;

	switch (mode) {
		case GIT_TREEWALK_POST:
			error = tree_walk_post(tree, callback, &root_path, payload);
			break;

		case GIT_TREEWALK_PRE:
			error = git__throw(GIT_ENOTIMPLEMENTED,
				"Preorder tree walking is still not implemented");
			break;

		default:
			error = git__throw(GIT_EINVALIDARGS,
				"Invalid walking mode for tree walk");
			break;
	}

	git_buf_free(&root_path);

	return error;
}

static int tree_entry_cmp(const git_tree_entry *a, const git_tree_entry *b)
{
	int ret;

	ret = a->attr - b->attr;
	if (ret != 0)
		return ret;

	return git_oid_cmp(&a->oid, &b->oid);
}

static void mark_del(git_tree_diff_data *diff, git_tree_entry *entry)
{
	diff->old_attr = entry->attr;
	git_oid_cpy(&diff->old_oid, &entry->oid);
	diff->path = entry->filename;
	diff->status |= GIT_STATUS_DELETED;
}

static void mark_add(git_tree_diff_data *diff, git_tree_entry *entry)
{
	diff->new_attr = entry->attr;
	git_oid_cpy(&diff->new_oid, &entry->oid);
	diff->path = entry->filename;
	diff->status |= GIT_STATUS_ADDED;
}

static int signal_additions(git_tree *tree, int start, int end, git_tree_diff_cb cb, void *data)
{
	git_tree_diff_data diff;
	git_tree_entry *entry;
	int i, error;

	if (end < 0)
		end = git_tree_entrycount(tree);

	for (i = start; i < end; ++i) {
		memset(&diff, 0x0, sizeof(git_tree_diff_data));
		entry = git_vector_get(&tree->entries, i);
		mark_add(&diff, entry);

		error = cb(&diff, data);
		if (error < GIT_SUCCESS)
			return error;
	}

	return GIT_SUCCESS;
}

static int signal_addition(git_tree_entry *entry, git_tree_diff_cb cb, void *data)
{
	git_tree_diff_data diff;

	memset(&diff, 0x0, sizeof(git_tree_diff_data));

	mark_add(&diff, entry);

	return cb(&diff, data);
}

static int signal_deletions(git_tree *tree, int start, int end, git_tree_diff_cb cb, void *data)
{
	git_tree_diff_data diff;
	git_tree_entry *entry;
	int i, error;

	if (end < 0)
		end = git_tree_entrycount(tree);

	for (i = start; i < end; ++i) {
		memset(&diff, 0x0, sizeof(git_tree_diff_data));
		entry = git_vector_get(&tree->entries, i);
		mark_del(&diff, entry);

		error = cb(&diff, data);
		if (error < GIT_SUCCESS)
			return error;
	}

	return GIT_SUCCESS;
}

static int signal_deletion(git_tree_entry *entry, git_tree_diff_cb cb, void *data)
{
	git_tree_diff_data diff;

	memset(&diff, 0x0, sizeof(git_tree_diff_data));

	mark_del(&diff, entry);

	return cb(&diff, data);
}

static int signal_modification(git_tree_entry *a, git_tree_entry *b,
							   git_tree_diff_cb cb, void *data)
{
	git_tree_diff_data diff;

	memset(&diff, 0x0, sizeof(git_tree_diff_data));

	mark_del(&diff, a);
	mark_add(&diff, b);

	return cb(&diff, data);
}

int git_tree_diff(git_tree *a, git_tree *b, git_tree_diff_cb cb, void *data)
{
	unsigned int i_a = 0, i_b = 0; /* Counters for trees a and b */
	git_tree_entry *entry_a = NULL, *entry_b = NULL;
	git_tree_diff_data diff;
	int error = GIT_SUCCESS, cmp;

	while (1) {
		entry_a = a == NULL ? NULL : git_vector_get(&a->entries, i_a);
		entry_b = b == NULL ? NULL : git_vector_get(&b->entries, i_b);

		if (!entry_a && !entry_b)
			goto exit;

		memset(&diff, 0x0, sizeof(git_tree_diff_data));

		/*
		 * We've run out of tree on one side so the rest of the
		 * entries on the tree with remaining entries are all
		 * deletions or additions.
		 */
		if (entry_a && !entry_b)
			return signal_deletions(a, i_a, -1, cb, data);
		if (!entry_a && entry_b)
			return signal_additions(b, i_b, -1, cb, data);

		/*
		 * Both trees are sorted with git's almost-alphabetical
		 * sorting, so a comparison value < 0 means the entry was
		 * deleted in the right tree. > 0 means the entry was added.
		 */
		cmp = entry_sort_cmp(entry_a, entry_b);

		if (cmp == 0) {
			i_a++;
			i_b++;

			/* If everything's the same, jump to next pair */
			if (!tree_entry_cmp(entry_a, entry_b))
				continue;

			/* If they're not both dirs or both files, it's add + del */
			if (S_ISDIR(entry_a->attr) != S_ISDIR(entry_b->attr)) {
				if ((error = signal_addition(entry_a, cb, data)) < 0)
					goto exit;
				if ((error = signal_deletion(entry_b, cb, data)) < 0)
					goto exit;
			}

			/* Otherwise consider it a modification */
			if ((error = signal_modification(entry_a, entry_b, cb, data)) < 0)
				goto exit;

		} else if (cmp < 0) {
			i_a++;
			if ((error = signal_deletion(entry_a, cb, data)) < 0)
				goto exit;
		} else if (cmp > 0) {
			i_b++;
			if ((error = signal_addition(entry_b, cb, data)) < 0)
				goto exit;
		}
	}

exit:
	return error;
}

struct diff_index_cbdata {
	git_index *index;
	unsigned int i;
	git_tree_diff_cb cb;
	void *data;
};

static int cmp_tentry_ientry(git_tree_entry *tentry, git_index_entry *ientry)
{
	int cmp;

	cmp = tentry->attr - ientry->mode;
	if (cmp != 0)
		return cmp;

	return git_oid_cmp(&tentry->oid, &ientry->oid);
}

static void make_tentry(git_tree_entry *tentry, git_index_entry *ientry)
{
	char *last_slash;

	memset(tentry, 0x0, sizeof(git_tree_entry));
	tentry->attr = ientry->mode;

	last_slash = strrchr(ientry->path, '/');
	if (last_slash)
		last_slash++;
	else
		last_slash = ientry->path;
	tentry->filename = last_slash;

	git_oid_cpy(&tentry->oid, &ientry->oid);
	tentry->filename_len = strlen(tentry->filename);
}

static int diff_index_cb(const char *root, git_tree_entry *tentry, void *data)
{
	struct diff_index_cbdata *cbdata = (struct diff_index_cbdata *) data;
	git_index_entry *ientry = git_index_get(cbdata->index, cbdata->i);
	git_tree_entry fake_entry;
	git_buf fn_buf = GIT_BUF_INIT;
	int cmp, error = GIT_SUCCESS;

	if (entry_is_tree(tentry))
		return GIT_SUCCESS;

	git_buf_puts(&fn_buf, root);
	git_buf_puts(&fn_buf, tentry->filename);

	if (!ientry) {
		error = signal_deletion(tentry, cbdata->cb, cbdata->data);
		goto exit;
	}

	/* Like with 'git diff-index', the index is the right side*/
	cmp = strcmp(git_buf_cstr(&fn_buf), ientry->path);
	git_buf_free(&fn_buf);
	if (cmp == 0) {
		cbdata->i++;
		if (!cmp_tentry_ientry(tentry, ientry))
			goto exit;
		/* modification */
		make_tentry(&fake_entry, ientry);
		if ((error = signal_modification(tentry, &fake_entry, cbdata->cb, cbdata->data)) < 0)
			goto exit;
	} else if (cmp < 0) {
		/* deletion */
		memcpy(&fake_entry, tentry, sizeof(git_tree_entry));
		if ((error = signal_deletion(tentry, cbdata->cb, cbdata->data)) < 0)
			goto exit;
	} else {
		/* addition */
		cbdata->i++;
		make_tentry(&fake_entry, ientry);
		if ((error = signal_addition(&fake_entry, cbdata->cb, cbdata->data)) < 0)
			goto exit;
		/*
		 * The index has an addition. This means that we need to use
		 * the next entry in the index without advancing the tree
		 * walker, so call ourselves with the same tree state.
		 */
		if ((error = diff_index_cb(root, tentry, data)) < 0)
			goto exit;
	}

 exit:
	return error;
}

int git_tree_diff_index_recursive(git_tree *tree, git_index *index, git_tree_diff_cb cb, void *data)
{
	struct diff_index_cbdata cbdata;
	git_buf dummy_path = GIT_BUF_INIT;

	cbdata.index = index;
	cbdata.i = 0;
	cbdata.cb = cb;
	cbdata.data = data;

	return tree_walk_post(tree, diff_index_cb, &dummy_path, &cbdata);
}
