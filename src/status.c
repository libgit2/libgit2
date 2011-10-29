/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "git2.h"
#include "fileops.h"
#include "hash.h"
#include "vector.h"
#include "tree.h"
#include "git2/status.h"
#include "repository.h"

struct status_entry {
	git_index_time mtime;

	git_oid head_oid;
	git_oid index_oid;
	git_oid wt_oid;

	unsigned int status_flags:6;

	char path[GIT_FLEX_ARRAY]; /* more */
};

static struct status_entry *status_entry_new(git_vector *entries, const char *path)
{
	struct status_entry *e = git__malloc(sizeof(*e) + strlen(path) + 1);
	if (e == NULL)
		return NULL;

	memset(e, 0x0, sizeof(*e));

	if (entries != NULL)
		git_vector_insert(entries, e);

	strcpy(e->path, path);

	return e;
}

GIT_INLINE(void) status_entry_update_from_tree_entry(struct status_entry *e, const git_tree_entry *tree_entry)
{
	assert(e && tree_entry);

	git_oid_cpy(&e->head_oid, &tree_entry->oid);
}

GIT_INLINE(void) status_entry_update_from_index_entry(struct status_entry *e, const git_index_entry *index_entry)
{
	assert(e && index_entry);

	git_oid_cpy(&e->index_oid, &index_entry->oid);
	e->mtime = index_entry->mtime;
}

static void status_entry_update_from_index(struct status_entry *e, git_index *index)
{
	int idx;
	git_index_entry *index_entry;

	assert(e && index);

	idx = git_index_find(index, e->path);
	if (idx < 0)
		return;

	index_entry = git_index_get(index, idx);

	status_entry_update_from_index_entry(e, index_entry);
}

static int status_entry_update_from_workdir(struct status_entry *e, char* full_path)
{
	struct stat filest;

	if (p_stat(full_path, &filest) < GIT_SUCCESS)
		return git__throw(GIT_EOSERR, "Failed to determine status of file '%s'. Can't read file", full_path);

	if (e->mtime.seconds == (git_time_t)filest.st_mtime)
		git_oid_cpy(&e->wt_oid, &e->index_oid);
	else
		git_odb_hashfile(&e->wt_oid, full_path, GIT_OBJ_BLOB);

	return GIT_SUCCESS;
}

static int status_entry_update_flags(struct status_entry *e)
{
	git_oid zero;
	int head_zero, index_zero, wt_zero;

	memset(&zero, 0x0, sizeof(git_oid));

	head_zero = git_oid_cmp(&zero, &e->head_oid);
	index_zero = git_oid_cmp(&zero, &e->index_oid);
	wt_zero = git_oid_cmp(&zero, &e->wt_oid);

	if (head_zero == 0 && index_zero == 0 && wt_zero == 0)
		return GIT_ENOTFOUND;

	if (head_zero == 0 && index_zero != 0)
		e->status_flags |= GIT_STATUS_INDEX_NEW;
	else if (index_zero == 0 && head_zero != 0)
		e->status_flags |= GIT_STATUS_INDEX_DELETED;
	else if (git_oid_cmp(&e->head_oid, &e->index_oid) != 0)
		e->status_flags |= GIT_STATUS_INDEX_MODIFIED;

	if (index_zero == 0 && wt_zero != 0)
		e->status_flags |= GIT_STATUS_WT_NEW;
	else if (wt_zero == 0 && index_zero != 0)
		e->status_flags |= GIT_STATUS_WT_DELETED;
	else if (git_oid_cmp(&e->index_oid, &e->wt_oid) != 0)
		e->status_flags |= GIT_STATUS_WT_MODIFIED;

	return GIT_SUCCESS;
}

struct status_st {
	git_vector *vector;
	git_index *index;
	git_tree *tree;

	int workdir_path_len;
	char* head_tree_relative_path;
	int head_tree_relative_path_len;
	unsigned int tree_position;
	unsigned int index_position;
	int is_dir:1;
};

static int retrieve_head_tree(git_tree **tree_out, git_repository *repo)
{
	git_reference *resolved_head_ref;
	git_commit *head_commit = NULL;
	git_tree *tree;
	int error = GIT_SUCCESS;

	*tree_out = NULL;

	error = git_repository_head(&resolved_head_ref, repo);
	/*
	 * We assume that a situation where HEAD exists but can not be resolved is valid.
	 * A new repository fits this description for instance.
	 */
	if (error == GIT_ENOTFOUND)
		return GIT_SUCCESS;
	if (error < GIT_SUCCESS)
		return git__rethrow(error, "HEAD can't be resolved");

	if ((error = git_commit_lookup(&head_commit, repo, git_reference_oid(resolved_head_ref))) < GIT_SUCCESS)
		return git__rethrow(error, "The tip of HEAD can't be retrieved");

	if ((error = git_commit_tree(&tree, head_commit)) < GIT_SUCCESS) {
		error = git__rethrow(error, "The tree of HEAD can't be retrieved");
		goto exit;
	}

	*tree_out = tree;

exit:
	git_commit_close(head_commit);
	return error;
}

enum path_type {
	GIT_STATUS_PATH_NULL,
	GIT_STATUS_PATH_IGNORE,
	GIT_STATUS_PATH_FILE,
	GIT_STATUS_PATH_FOLDER,
};

static int dirent_cb(void *state, char *full_path);
static int alphasorted_futils_direach(
	char *path, size_t path_sz,
	int (*fn)(void *, char *), void *arg);

static int process_folder(struct status_st *st, const git_tree_entry *tree_entry, char *full_path, enum path_type path_type)
{
	git_object *subtree = NULL;
	git_tree *pushed_tree = NULL;
	int error, pushed_tree_position = 0;
	git_otype tree_entry_type = GIT_OBJ_BAD;

	if (tree_entry != NULL) {
		tree_entry_type = git_tree_entry_type(tree_entry);

		switch (tree_entry_type) {
		case GIT_OBJ_TREE:
			error = git_tree_entry_2object(&subtree, ((git_object *)(st->tree))->repo, tree_entry);
			pushed_tree = st->tree;
			pushed_tree_position = st->tree_position;
			st->tree = (git_tree *)subtree;
			st->tree_position = 0;
			st->head_tree_relative_path_len += 1 + tree_entry->filename_len; /* path + '/' + name */
			break;

		case GIT_OBJ_BLOB:
			/* No op */
			break;

		default:
			error = git__throw(GIT_EINVALIDTYPE, "Unexpected tree entry type");	/* TODO: How should we deal with submodules? */
		}
	}

	if (full_path != NULL && path_type == GIT_STATUS_PATH_FOLDER)
		error = alphasorted_futils_direach(full_path, GIT_PATH_MAX, dirent_cb, st);
	else {
		error = dirent_cb(st, NULL);
	}

	if (tree_entry_type == GIT_OBJ_TREE) {
		git_object_close(subtree);
		st->head_tree_relative_path_len -= 1 + tree_entry->filename_len;
		st->tree = pushed_tree;
		st->tree_position = pushed_tree_position;
		st->tree_position++;
	}

	return error;
}

static int store_if_changed(struct status_st *st, struct status_entry *e)
{
	int error;
	if ((error = status_entry_update_flags(e)) < GIT_SUCCESS)
			return git__throw(error, "Failed to process the file '%s'. It doesn't exist in the workdir, in the HEAD nor in the index", e->path);

	if (e->status_flags == GIT_STATUS_CURRENT) {
		git__free(e);
		return GIT_SUCCESS;
	}

	return git_vector_insert(st->vector, e);
}

static int determine_status(struct status_st *st,
	int in_head, int in_index, int in_workdir,
	const git_tree_entry *tree_entry,
	const git_index_entry *index_entry,
	char *full_path,
	const char *status_path,
	enum path_type path_type)
{
	struct status_entry *e;
	int error = GIT_SUCCESS;
	git_otype tree_entry_type = GIT_OBJ_BAD;

	if (tree_entry != NULL)
		tree_entry_type = git_tree_entry_type(tree_entry);

	/* If we're dealing with a directory in the workdir, let's recursively tackle it first */
	if (path_type == GIT_STATUS_PATH_FOLDER)
		return process_folder(st, tree_entry, full_path, path_type);

	/* Are we dealing with a file somewhere? */
	if (in_workdir || in_index || (in_head && tree_entry_type == GIT_OBJ_BLOB)) {
		e = status_entry_new(NULL, status_path);

		if (in_head && tree_entry_type == GIT_OBJ_BLOB) {
			status_entry_update_from_tree_entry(e, tree_entry);
			st->tree_position++;
		}

		if (in_index) {
			status_entry_update_from_index_entry(e, index_entry);
			st->index_position++;
		}

		if (in_workdir)
			if ((error = status_entry_update_from_workdir(e, full_path)) < GIT_SUCCESS)
				return error;	/* The callee has already set the error message */

		return store_if_changed(st, e);
	}

	/* Last option, we're dealing with a leftover folder tree entry */
	assert(in_head && !in_index && !in_workdir && (tree_entry_type == GIT_OBJ_TREE));
	return process_folder(st, tree_entry, full_path, path_type);
}

static int path_type_from(char *full_path, int is_dir)
{
	if (full_path == NULL)
		return GIT_STATUS_PATH_NULL;

	if (!is_dir)
		return GIT_STATUS_PATH_FILE;

	if (!git__suffixcmp(full_path, "/" DOT_GIT "/"))
		return GIT_STATUS_PATH_IGNORE;

	return GIT_STATUS_PATH_FOLDER;
}

static const char *status_path(const char *first, const char *second, const char *third)
{
	/* At least one of them can not be NULL */
	assert(first != NULL || second != NULL || third != NULL);

	/* TODO: Fixme. Ensure that when non null, they're all equal */
	if (first != NULL)
		return first;

	if (second != NULL)
		return second;

	return third;
}

static int compare(const char *left, const char *right)
{
	if (left == NULL && right == NULL)
		return 0;

	if (left == NULL)
		return 1;

	if (right == NULL)
		return -1;

	return strcmp(left, right);
}

/* Greatly inspired from JGit IndexTreeWalker */
/* https://github.com/spearce/jgit/blob/ed47e29c777accfa78c6f50685a5df2b8f5b8ff5/org.spearce.jgit/src/org/spearce/jgit/lib/IndexTreeWalker.java#L88 */

static int dirent_cb(void *state, char *a)
{
	const git_tree_entry *m;
	const git_index_entry *entry;
	enum path_type path_type;
	int cmpma, cmpmi, cmpai, error;
	const char *pm, *pa, *pi;
	const char *m_name, *i_name, *a_name;

	struct status_st *st = (struct status_st *)state;

	path_type = path_type_from(a, st->is_dir);

	if (path_type == GIT_STATUS_PATH_IGNORE)
		return GIT_SUCCESS;	/* Let's skip the ".git" directory */

	a_name = (path_type != GIT_STATUS_PATH_NULL) ? a + st->workdir_path_len : NULL;

	while (1) {
		if (st->tree == NULL)
			m = NULL;
		else
			m = git_tree_entry_byindex(st->tree, st->tree_position);

		entry = git_index_get(st->index, st->index_position);

		if ((m == NULL) && (a == NULL) && (entry == NULL))
			return GIT_SUCCESS;

		if (m != NULL) {
			st->head_tree_relative_path[st->head_tree_relative_path_len] = '\0';

			/* When the tree entry is a folder, append a forward slash to its name */
			if (git_tree_entry_type(m) == GIT_OBJ_TREE)
				git_path_join_n(st->head_tree_relative_path, 3, st->head_tree_relative_path, m->filename, "");
			else
				git_path_join(st->head_tree_relative_path, st->head_tree_relative_path, m->filename);
		
			m_name = st->head_tree_relative_path;
		} else
			m_name = NULL;

		i_name = (entry != NULL) ? entry->path : NULL;

		cmpma = compare(m_name, a_name);
		cmpmi = compare(m_name, i_name);
		cmpai = compare(a_name, i_name);

		pm = ((cmpma <= 0) && (cmpmi <= 0)) ? m_name : NULL;
		pa = ((cmpma >= 0) && (cmpai <= 0)) ? a_name : NULL;
		pi = ((cmpmi >= 0) && (cmpai >= 0)) ? i_name : NULL;

		if((error = determine_status(st, pm != NULL, pi != NULL, pa != NULL, m, entry, a, status_path(pm, pi, pa), path_type)) < GIT_SUCCESS)
			return git__rethrow(error, "An error occured while determining the status of '%s'", a);

		if ((pa != NULL) || (path_type == GIT_STATUS_PATH_FOLDER))
			return GIT_SUCCESS;
	}
}

static int status_cmp(const void *a, const void *b)
{
	const struct status_entry *entry_a = (const struct status_entry *)(a);
	const struct status_entry *entry_b = (const struct status_entry *)(b);

	return strcmp(entry_a->path, entry_b->path);
}

#define DEFAULT_SIZE 16

int git_status_foreach(git_repository *repo, int (*callback)(const char *, unsigned int, void *), void *payload)
{
	git_vector entries;
	git_index *index = NULL;
	char temp_path[GIT_PATH_MAX];
	char tree_path[GIT_PATH_MAX] = "";
	struct status_st dirent_st;
	int error = GIT_SUCCESS;
	unsigned int i;
	git_tree *tree;
	struct status_entry *e;

	if ((error = git_repository_index(&index, repo)) < GIT_SUCCESS) {
		return git__rethrow(error, "Failed to determine statuses. Index can't be opened");
	}

	if ((error = retrieve_head_tree(&tree, repo)) < GIT_SUCCESS) {
		error = git__rethrow(error, "Failed to determine statuses");
		goto exit;
	}

	git_vector_init(&entries, DEFAULT_SIZE, status_cmp);

	dirent_st.workdir_path_len = strlen(repo->path_workdir);
	dirent_st.tree_position = 0;
	dirent_st.index_position = 0;
	dirent_st.tree = tree;
	dirent_st.index = index;
	dirent_st.vector = &entries;
	dirent_st.head_tree_relative_path = tree_path;
	dirent_st.head_tree_relative_path_len = 0;
	dirent_st.is_dir = 1;

	strcpy(temp_path, repo->path_workdir);

	if (git_futils_isdir(temp_path)) {
		error = git__throw(GIT_EINVALIDPATH, "Failed to determine status of file '%s'. Provided path doesn't lead to a folder", temp_path);
		goto exit;
	}

	if ((error = alphasorted_futils_direach(temp_path, sizeof(temp_path), dirent_cb, &dirent_st)) < GIT_SUCCESS)
		error = git__rethrow(error, "Failed to determine statuses. An error occured while processing the working directory");

	if ((error == GIT_SUCCESS) && ((error = dirent_cb(&dirent_st, NULL)) < GIT_SUCCESS))
		error = git__rethrow(error, "Failed to determine statuses. An error occured while post-processing the HEAD tree and the index");

	for (i = 0; i < entries.length; ++i) {
		e = (struct status_entry *)git_vector_get(&entries, i);

		if (error == GIT_SUCCESS) {
			error = callback(e->path, e->status_flags, payload);
			if (error < GIT_SUCCESS)
				error = git__rethrow(error, "Failed to determine statuses. User callback failed");
		}

		git__free(e);
	}

exit:
	git_vector_free(&entries);
	git_tree_close(tree);
	git_index_free(index);
	return error;
}

static int recurse_tree_entry(git_tree *tree, struct status_entry *e, const char *path)
{
	char *dir_sep;
	const git_tree_entry *tree_entry;
	git_tree *subtree;
	int error = GIT_SUCCESS;

	dir_sep = strchr(path, '/');
	if (!dir_sep) {
		tree_entry = git_tree_entry_byname(tree, path);
		if (tree_entry == NULL)
			return GIT_SUCCESS;	/* The leaf doesn't exist in the tree*/

		status_entry_update_from_tree_entry(e, tree_entry);
		return GIT_SUCCESS;
	}

	/* Retrieve subtree name */
	*dir_sep = '\0';

	tree_entry = git_tree_entry_byname(tree, path);
	if (tree_entry == NULL)
		return GIT_SUCCESS;	/* The subtree doesn't exist in the tree*/

	*dir_sep = '/';

	/* Retreive subtree */
	if ((error = git_tree_lookup(&subtree, tree->object.repo, &tree_entry->oid)) < GIT_SUCCESS)
		return git__throw(GIT_EOBJCORRUPTED, "Can't find tree object '%s'", tree_entry->filename);

	error = recurse_tree_entry(subtree, e, dir_sep+1);
	git_tree_close(subtree);
	return error;
}

int git_status_file(unsigned int *status_flags, git_repository *repo, const char *path)
{
	struct status_entry *e;
	git_index *index = NULL;
	char temp_path[GIT_PATH_MAX];
	int error = GIT_SUCCESS;
	git_tree *tree = NULL;

	assert(status_flags && repo && path);

	git_path_join(temp_path, repo->path_workdir, path);
	if (git_futils_isdir(temp_path) == GIT_SUCCESS)
		return git__throw(GIT_EINVALIDPATH, "Failed to determine status of file '%s'. Provided path leads to a folder, not a file", path);

	e = status_entry_new(NULL, path);
	if (e == NULL)
		return GIT_ENOMEM;

	/* Find file in Workdir */
	if (git_futils_exists(temp_path) == GIT_SUCCESS) {
		if ((error = status_entry_update_from_workdir(e, temp_path)) < GIT_SUCCESS)
			goto exit;	/* The callee has already set the error message */
	}

	/* Find file in Index */
	if ((error = git_repository_index(&index, repo)) < GIT_SUCCESS) {
		error = git__rethrow(error, "Failed to determine status of file '%s'. Index can't be opened", path);
		goto exit;
	}

	status_entry_update_from_index(e, index);
	git_index_free(index);

	if ((error = retrieve_head_tree(&tree, repo)) < GIT_SUCCESS) {
		error = git__rethrow(error, "Failed to determine status of file '%s'", path);
		goto exit;
	}

	/* If the repository is not empty, try and locate the file in HEAD */
	if (tree != NULL) {
		strcpy(temp_path, path);

		error = recurse_tree_entry(tree, e, temp_path);
		if (error < GIT_SUCCESS) {
			error = git__rethrow(error, "Failed to determine status of file '%s'. An error occured while processing the tree", path);
			goto exit;
		}
	}

	/* Determine status */
	if ((error = status_entry_update_flags(e)) < GIT_SUCCESS) {
		error = git__throw(error, "Nonexistent file");
		goto exit;
	}

	*status_flags = e->status_flags;

exit:
	git_tree_close(tree);
	git__free(e);
	return error;
}

/*
 * git_futils_direach is not supposed to return entries in an ordered manner.
 * alphasorted_futils_direach wraps git_futils_direach and invokes the callback
 * function by passing it alphabeticcally sorted paths parameters.
 *
 */

struct alphasorted_dirent_info {
	int is_dir;

	char path[GIT_FLEX_ARRAY]; /* more */
};

static struct alphasorted_dirent_info *alphasorted_dirent_info_new(const char *path)
{
	int is_dir, size;
	struct alphasorted_dirent_info *di;

	is_dir = git_futils_isdir(path) == GIT_SUCCESS ? 1 : 0;
	size = sizeof(*di) + (is_dir ? GIT_PATH_MAX : strlen(path)) + 2;

	di = git__malloc(size);
	if (di == NULL)
		return NULL;

	memset(di, 0x0, size);

	strcpy(di->path, path);

	if (is_dir) {
		di->is_dir = 1;

		/* 
		 * Append a forward slash to the name to force folders 
		 * to be ordered in a similar way than in a tree
		 *
		 * The file "subdir" should appear before the file "subdir.txt"
		 * The folder "subdir" should appear after the file "subdir.txt"
		 */
		di->path[strlen(path)] = '/';
	}

	return di;
}

static int alphasorted_dirent_info_cmp(const void *a, const void *b)
{
	struct alphasorted_dirent_info *stra = (struct alphasorted_dirent_info *)a;
	struct alphasorted_dirent_info *strb = (struct alphasorted_dirent_info *)b;

	return strcmp(stra->path, strb->path);
}

static int alphasorted_dirent_cb(void *state, char *full_path)
{
	struct alphasorted_dirent_info *entry;
	git_vector *entry_names;

	entry_names = (git_vector *)state;
	entry = alphasorted_dirent_info_new(full_path);

	if (entry == NULL)
		return GIT_ENOMEM;

	if (git_vector_insert(entry_names, entry) < GIT_SUCCESS) {
		git__free(entry);
		return GIT_ENOMEM;
	}

	return GIT_SUCCESS;
}

static int alphasorted_futils_direach(
	char *path,
	size_t path_sz,
	int (*fn)(void *, char *),
	void *arg)
{
	struct alphasorted_dirent_info *entry;
	git_vector entry_names;
	unsigned int idx;
	int error = GIT_SUCCESS;

	if (git_vector_init(&entry_names, 16, alphasorted_dirent_info_cmp) < GIT_SUCCESS)
		return GIT_ENOMEM;

	error = git_futils_direach(path, path_sz, alphasorted_dirent_cb, &entry_names);

	git_vector_sort(&entry_names);

	for (idx = 0; idx < entry_names.length; ++idx) {
		entry = (struct alphasorted_dirent_info *)git_vector_get(&entry_names, idx);

		if (error == GIT_SUCCESS) {
			((struct status_st *)arg)->is_dir = entry->is_dir;
			error = fn(arg, entry->path);
		}

		git__free(entry);
	}

	git_vector_free(&entry_names);
	return error;
}
