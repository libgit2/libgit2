/*
 * Copyright (C) 2009-2012 the libgit2 contributors
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
#include "ignore.h"

struct status_entry {
	git_index_time mtime;

	git_oid head_oid;
	git_oid index_oid;
	git_oid wt_oid;

	unsigned int status_flags;

	char path[GIT_FLEX_ARRAY]; /* more */
};

static struct status_entry *status_entry_new(git_vector *entries, const char *path)
{
	struct status_entry *e = git__calloc(sizeof(*e) + strlen(path) + 1, 1);
	if (e == NULL)
		return NULL;

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

static int status_entry_update_from_workdir(struct status_entry *e, const char* full_path)
{
	struct stat filest;

	if (p_stat(full_path, &filest) < 0) {
		giterr_set(GITERR_OS, "Cannot access file '%s'", full_path);
		return GIT_ENOTFOUND;
	}

	if (e->mtime.seconds == (git_time_t)filest.st_mtime)
		git_oid_cpy(&e->wt_oid, &e->index_oid);
	else
		git_odb_hashfile(&e->wt_oid, full_path, GIT_OBJ_BLOB);

	return 0;
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

	return 0;
}

static int status_entry_is_ignorable(struct status_entry *e)
{
	/* don't ignore files that exist in head or index already */
	return (e->status_flags == GIT_STATUS_WT_NEW);
}

static int status_entry_update_ignore(struct status_entry *e, git_ignores *ignores, const char *path)
{
	int ignored;

	if (git_ignore__lookup(ignores, path, &ignored) < 0)
		return -1;

	if (ignored)
		/* toggle off WT_NEW and on IGNORED */
		e->status_flags =
			(e->status_flags & ~GIT_STATUS_WT_NEW) | GIT_STATUS_IGNORED;

	return 0;
}

struct status_st {
	git_repository *repo;
	git_vector *vector;
	git_index *index;
	git_tree *tree;
	git_ignores *ignores;

	int workdir_path_len;
	git_buf head_tree_relative_path;
	int head_tree_relative_path_len;
	unsigned int tree_position;
	unsigned int index_position;
};

static int retrieve_head_tree(git_tree **tree_out, git_repository *repo)
{
	git_reference *resolved_head_ref;
	git_commit *head_commit = NULL;
	git_tree *tree;
	int error = 0;

	*tree_out = NULL;

	if ((error = git_repository_head(&resolved_head_ref, repo)) < 0) {
		/* Assume that a situation where HEAD exists but can not be resolved
		 * is valid.  A new repository fits this description for instance.
		 */
		if (error == GIT_ENOTFOUND)
			return 0;
		return error;
	}

	if ((error = git_commit_lookup(
		&head_commit, repo, git_reference_oid(resolved_head_ref))) < 0)
		return error;

	git_reference_free(resolved_head_ref);

	if ((error = git_commit_tree(&tree, head_commit)) == 0)
		*tree_out = tree;

	git_commit_free(head_commit);
	return error;
}

enum path_type {
	GIT_STATUS_PATH_NULL,
	GIT_STATUS_PATH_IGNORE,
	GIT_STATUS_PATH_FILE,
	GIT_STATUS_PATH_FOLDER,
};

typedef int (*alphasorted_direach_cb)(git_vector *, void *);

static int alphasorted_futils_direach(
	const char *path, alphasorted_direach_cb direach_cb, void *arg);

static int worktreewalker_cb(git_vector *wt_entries, void *state);

static int process_folder(
	struct status_st *st,
	const git_tree_entry *tree_entry,
	const char *full_path)
{
	git_object *subtree = NULL;
	git_tree *pushed_tree = NULL;
	int error, pushed_tree_position = 0;
	git_otype tree_entry_type = GIT_OBJ_BAD;

	assert(full_path || tree_entry);

	if (tree_entry != NULL) {
		tree_entry_type = git_tree_entry_type(tree_entry);

		switch (tree_entry_type) {
		case GIT_OBJ_TREE:
			if ((error = git_tree_entry_2object(&subtree, ((git_object *)(st->tree))->repo, tree_entry)) < 0)
				return error;

			pushed_tree = st->tree;
			pushed_tree_position = st->tree_position;
			st->tree = (git_tree *)subtree;
			st->tree_position = 0;
			st->head_tree_relative_path_len += 1 + tree_entry->filename_len; /* path + '/' + name */
			break;

		case GIT_OBJ_BLOB:
			/* No op */
			break;

		case GIT_OBJ_COMMIT:
			/* TODO: proper submodule support */
			break;

		default:
			giterr_set(GITERR_REPOSITORY, "Unexpected tree entry type");
			return -1;
		}
	}

	if (full_path != NULL) {
		git_ignores ignores, *old_ignores;

		if ((error = git_ignore__for_path(st->repo,
			full_path + st->workdir_path_len, &ignores)) == 0) {

			old_ignores = st->ignores;
			st->ignores = &ignores;

			error = alphasorted_futils_direach(full_path, worktreewalker_cb, st);

			git_ignore__free(st->ignores);
			st->ignores = old_ignores;
		}
	} else {
		error = worktreewalker_cb(NULL, st);
	}

	if (tree_entry_type == GIT_OBJ_TREE) {
		git_object_free(subtree);
		st->head_tree_relative_path_len -= 1 + tree_entry->filename_len;
		st->tree = pushed_tree;
		st->tree_position = pushed_tree_position;
	}

	return error;
}

static int store_if_changed(struct status_st *st, struct status_entry *e)
{
	int error = status_entry_update_flags(e);
	if (error < 0)
		return error;

	if (status_entry_is_ignorable(e) &&
		(error = status_entry_update_ignore(e, st->ignores, e->path)) < 0)
		return error;

	if (e->status_flags == GIT_STATUS_CURRENT) {
		git__free(e);
		return 0;
	}

	return git_vector_insert(st->vector, e);
}

static int path_type_from(const char *workdir_path)
{
	if (workdir_path == NULL)
		return GIT_STATUS_PATH_NULL;

	if (strcmp(workdir_path, DOT_GIT "/") == 0)
		return GIT_STATUS_PATH_IGNORE;

	if (git__suffixcmp(workdir_path, "/" DOT_GIT "/") == 0)
		return GIT_STATUS_PATH_IGNORE;

	if (git__suffixcmp(workdir_path, "/") == 0)
		return GIT_STATUS_PATH_FOLDER;

	return GIT_STATUS_PATH_FILE;
}

static int determine_status(
	struct status_st *st,
	int in_head, int in_index, int in_workdir,
	const git_tree_entry *tree_entry,
	const git_index_entry *index_entry,
	const char *full_path,
	const char *status_path)
{
	struct status_entry *e;
	int error = 0;
	git_otype tree_entry_type = GIT_OBJ_BAD;
	int path_type;

	path_type = path_type_from(status_path);

	assert(path_type == GIT_STATUS_PATH_FOLDER || path_type == GIT_STATUS_PATH_FILE);

	if (tree_entry != NULL)
		tree_entry_type = git_tree_entry_type(tree_entry);

	/* If we're dealing with a directory in the workdir and/or a
	 * tree, let's recursively tackle it first
	 */
	if (path_type == GIT_STATUS_PATH_FOLDER)
		return process_folder(st, in_head ? tree_entry : NULL, in_workdir ? full_path : NULL);

	/* Are we dealing with a file somewhere? */
	if (in_workdir || in_index || (in_head && tree_entry_type == GIT_OBJ_BLOB)) {
		e = status_entry_new(NULL, status_path);

		if (in_head && tree_entry_type == GIT_OBJ_BLOB)
			status_entry_update_from_tree_entry(e, tree_entry);

		if (in_index)
			status_entry_update_from_index_entry(e, index_entry);

		if (in_workdir &&
			(error = status_entry_update_from_workdir(e, full_path)) < 0)
			return error;	/* The callee has already set the error message */

		return store_if_changed(st, e);
	}

	/* We're dealing with something else -- most likely a submodule;
	 * skip it for now */
	if (in_head)
		st->tree_position++;
	if (in_index)
		st->index_position++;
	return 0;
}

static const char *status_path(
	const char *first, const char *second, const char *third)
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

static int status_path_dirname(git_buf *parent_dir, const char *status_path)
{
	int error;

	if ((error = git_path_dirname_r(parent_dir, status_path)) < 0)
		return error;

	if (strcmp(git_buf_cstr(parent_dir), ".") == 0)
		git_buf_clear(parent_dir);
	else
		if ((error = git_buf_putc(parent_dir, '/')) < 0)
			return error;

	return 0;
}

GIT_INLINE(char *) worktree_movenext(git_vector *worktree_entries, unsigned int *idx)
{
	if (worktree_entries == NULL)
		return NULL;

	return (char *)git_vector_get(worktree_entries, (*idx)++);
}

GIT_INLINE(const git_tree_entry *) tree_currententry(git_tree *tree, unsigned int idx)
{
	if (tree == NULL)
		return NULL;

	return git_tree_entry_byindex(tree, idx);
}

GIT_INLINE(int) treeentry_prepend_path(
	git_buf *path_out,
	const git_tree_entry *treeentry,
	git_buf *head_tree_relative_path,
	int head_tree_relative_path_len
	)
{
	if (treeentry == NULL) {
		git_buf_clear(path_out);
		return 0;
	}

	git_buf_truncate(head_tree_relative_path,
						head_tree_relative_path_len);
	git_buf_joinpath(head_tree_relative_path,
						head_tree_relative_path->ptr, treeentry->filename);

	/* When the tree entry is a folder, append a forward slash to its name */
	if (git_tree_entry_type(treeentry) == GIT_OBJ_TREE)
		git_path_to_dir(head_tree_relative_path);

	if (git_buf_oom(head_tree_relative_path))
		return -1;

	git_buf_set(path_out, head_tree_relative_path->ptr, head_tree_relative_path->size);

	return 0;
}

GIT_INLINE(const char *) to_char(git_buf *buffer)
{
	if (buffer->size == 0)
		return NULL;

	return git_buf_cstr(buffer);
}

/* Greatly inspired from JGit IndexTreeWalker */
/* https://github.com/spearce/jgit/blob/ed47e29c777accfa78c6f50685a5df2b8f5b8ff5/org.spearce.jgit/src/org/spearce/jgit/lib/IndexTreeWalker.java#L88 */

static int worktreewalker_cb(git_vector *wt_entries, void *state)
{
	const git_tree_entry *m;
	const git_index_entry *entry;
	enum path_type wt_path_type;
	int cmpma, cmpmi, cmpai, error = 0, pushed_index_position;
	unsigned int i;
	const char *pm, *pa, *pi;
	const char *m_name, *i_name, *a_name;
	const char *path_to_process;
	struct status_st *st = (struct status_st *)state;
	git_buf parent_dir = GIT_BUF_INIT, treeentry_name = GIT_BUF_INIT;

	char *wt_entry;
	unsigned int wt_idx = 0;

	wt_entry = worktree_movenext(wt_entries, &wt_idx);
	m = tree_currententry(st->tree, st->tree_position);
	entry = git_index_get(st->index, st->index_position);

	while ((wt_entry != NULL || m != NULL || entry != NULL))
	{
		if (error < 0)
			goto exit;

		a_name = wt_entry != NULL ? wt_entry + st->workdir_path_len : NULL;
		wt_path_type = path_type_from(a_name);

		if (wt_path_type == GIT_STATUS_PATH_IGNORE) {
			git__free(wt_entry);
			wt_entry = worktree_movenext(wt_entries, &wt_idx);
			continue;
		}

		if ((error = treeentry_prepend_path(&treeentry_name, m, &st->head_tree_relative_path, st->head_tree_relative_path_len)) < 0)
			goto exit;

		m_name = to_char(&treeentry_name);
		i_name = (entry != NULL) ? entry->path : NULL;

		cmpma = compare(m_name, a_name);
		cmpmi = compare(m_name, i_name);
		cmpai = compare(a_name, i_name);

		pm = ((cmpma <= 0) && (cmpmi <= 0)) ? m_name : NULL;
		pa = ((cmpma >= 0) && (cmpai <= 0)) ? a_name : NULL;
		pi = ((cmpmi >= 0) && (cmpai >= 0)) ? i_name : NULL;

		path_to_process = status_path(pm, pi, pa);

		/* Did we escape from the folder being examined? */
		if ((parent_dir.size > 0)
			&& (git__prefixcmp(path_to_process, git_buf_cstr(&parent_dir)) != 0))
			goto exit;

		if ((error = status_path_dirname(&parent_dir, path_to_process)) < 0)
			goto exit;

		/* As the position of the index may move while determining status of entries
		 * of folders and trees, we store its current value
		 */
		pushed_index_position = st->index_position;

		if ((error = determine_status(st, pm != NULL, pi != NULL, pa != NULL,
				m, entry, wt_entry, path_to_process)) < 0)
			goto exit;

		if (m_name != NULL && strcmp(m_name, path_to_process) == 0)
			m = tree_currententry(st->tree, ++st->tree_position);

		if (pushed_index_position != st->index_position)
			/* Refresh the index entry as the current one has already
			 * been processed
			 */
			entry = git_index_get(st->index, st->index_position);
		else if (i_name != NULL && strcmp(i_name, path_to_process) == 0)
			entry = git_index_get(st->index, ++st->index_position);

		if (a_name != NULL && strcmp(a_name, path_to_process) == 0) {
			git__free(wt_entry);
			wt_entry = worktree_movenext(wt_entries, &wt_idx);
		}
	}

exit:
	git_buf_free(&parent_dir);
	git_buf_free(&treeentry_name);

	git__free(wt_entry);

	if (wt_entries != NULL)
		for (i = wt_idx; i < wt_entries->length; i++)
			git__free(git_vector_get(wt_entries, i));

	return error;
}

static int status_cmp(const void *a, const void *b)
{
	const struct status_entry *entry_a = (const struct status_entry *)(a);
	const struct status_entry *entry_b = (const struct status_entry *)(b);

	return strcmp(entry_a->path, entry_b->path);
}

#define DEFAULT_SIZE 16

int git_status_foreach(
	git_repository *repo,
	int (*callback)(const char *, unsigned int, void *),
	void *payload)
{
	git_vector entries;
	git_ignores ignores;
	git_index *index = NULL;
	struct status_st dirent_st = {0};
	int error = 0;
	unsigned int i;
	git_tree *tree;
	struct status_entry *e;
	const char *workdir;

	assert(repo);

	if ((workdir = git_repository_workdir(repo)) == NULL ||
		!git_path_isdir(workdir))
	{
		giterr_set(GITERR_OS, "Cannot get status - invalid working directory");
		return GIT_ENOTFOUND;
	}

	if ((error = git_repository_index__weakptr(&index, repo)) < 0 ||
		(error = retrieve_head_tree(&tree, repo)) < 0)
		return error;

	if ((error = git_vector_init(&entries, DEFAULT_SIZE, status_cmp)) < 0)
		goto exit;

	dirent_st.repo = repo;
	dirent_st.vector = &entries;
	dirent_st.index = index;
	dirent_st.index_position = 0;
	dirent_st.tree = tree;
	dirent_st.tree_position = 0;

	dirent_st.ignores = &ignores;
	dirent_st.workdir_path_len = strlen(workdir);
	git_buf_init(&dirent_st.head_tree_relative_path, 0);
	dirent_st.head_tree_relative_path_len = 0;

	if ((error = git_ignore__for_path(repo, "", dirent_st.ignores)) < 0)
		goto exit;

	error = alphasorted_futils_direach(workdir, worktreewalker_cb, &dirent_st);

	for (i = 0; i < entries.length; ++i) {
		e = (struct status_entry *)git_vector_get(&entries, i);

		if (!error)
			error = callback(e->path, e->status_flags, payload);

		git__free(e);
	}

exit:
	git_buf_free(&dirent_st.head_tree_relative_path);
	git_vector_free(&entries);
	git_ignore__free(&ignores);
	git_tree_free(tree);
	return error;
}

static int recurse_tree_entry(git_tree *tree, struct status_entry *e, const char *path)
{
	char *dir_sep;
	const git_tree_entry *tree_entry;
	git_tree *subtree;
	int error;

	dir_sep = strchr(path, '/');
	if (!dir_sep) {
		if ((tree_entry = git_tree_entry_byname(tree, path)) != NULL)
			/* The leaf exists in the tree*/
			status_entry_update_from_tree_entry(e, tree_entry);
		return 0;
	}

	/* Retrieve subtree name */
	*dir_sep = '\0';

	if ((tree_entry = git_tree_entry_byname(tree, path)) == NULL)
		return 0; /* The subtree doesn't exist in the tree*/

	*dir_sep = '/';

	/* Retreive subtree */
	error = git_tree_lookup(&subtree, tree->object.repo, &tree_entry->oid);
	if (!error) {
		error = recurse_tree_entry(subtree, e, dir_sep+1);
		git_tree_free(subtree);
	}

	return error;
}

int git_status_file(
	unsigned int *status_flags, git_repository *repo, const char *path)
{
	struct status_entry *e;
	git_index *index = NULL;
	git_buf temp_path = GIT_BUF_INIT;
	int error = 0;
	git_tree *tree = NULL;
	const char *workdir;

	assert(status_flags && repo && path);

	if ((workdir = git_repository_workdir(repo)) == NULL) {
		giterr_set(GITERR_OS, "Cannot get file status from bare repo");
		return GIT_ENOTFOUND;
	}

	if (git_buf_joinpath(&temp_path, workdir, path) < 0)
		return -1;

	if (git_path_isdir(temp_path.ptr)) {
		giterr_set(GITERR_OS, "Cannot get file status for directory '%s'", temp_path.ptr);
		git_buf_free(&temp_path);
		return GIT_ENOTFOUND;
	}

	e = status_entry_new(NULL, path);
	GITERR_CHECK_ALLOC(e);

	/* Find file in Workdir */
	if (git_path_exists(temp_path.ptr) == true &&
		(error = status_entry_update_from_workdir(e, temp_path.ptr)) < 0)
		goto cleanup;

	/* Find file in Index */
	if ((error = git_repository_index__weakptr(&index, repo)) < 0)
		goto cleanup;
	status_entry_update_from_index(e, index);

	/* Try to find file in HEAD */
	if ((error = retrieve_head_tree(&tree, repo)) < 0)
		goto cleanup;

	if (tree != NULL) {
		if ((error = git_buf_sets(&temp_path, path)) < 0 ||
			(error = recurse_tree_entry(tree, e, temp_path.ptr)) < 0)
			goto cleanup;
	}

	/* Determine status */
	if ((error = status_entry_update_flags(e)) < 0)
		giterr_set(GITERR_OS, "Cannot find file '%s' to determine status", path);

	if (!error && status_entry_is_ignorable(e)) {
		git_ignores ignores;

		if ((error = git_ignore__for_path(repo, path, &ignores)) == 0)
			error = status_entry_update_ignore(e, &ignores, path);

		git_ignore__free(&ignores);
	}

	if (!error)
		*status_flags = e->status_flags;

cleanup:
	git_buf_free(&temp_path);
	git_tree_free(tree);
	git__free(e);

	return error;
}

/*
 * git_path_direach is not supposed to return entries in an ordered manner.
 * alphasorted_futils_direach wraps git_path_dirload and invokes the
 * callback function by passing it alphabetically sorted path parameters.
 *
 */
static int alphasorted_futils_direach(
	const char *path,
	alphasorted_direach_cb direach_cb,
	void *arg)
{
	int error;
	char *entry;
	git_vector entry_names;
	unsigned int idx;

	if (git_vector_init(&entry_names, 16, git__strcmp_cb) < 0)
		return -1;

	if ((path != NULL) && ((error = git_path_dirload(path, 0, 1, &entry_names)) < 0))
		return error;

	git_vector_foreach(&entry_names, idx, entry) {
		if (git_path_isdir(entry)) {
			size_t entry_len = strlen(entry);

			/* dirload allocated 1 extra byte so there is space for slash */
			entry[entry_len++] = '/';
			entry[entry_len]   = '\0';
		}
	}

	git_vector_sort(&entry_names);

	error = direach_cb(&entry_names, arg);

	git_vector_free(&entry_names);

	return error;
}

int git_status_should_ignore(git_repository *repo, const char *path, int *ignored)
{
	int error;
	git_ignores ignores;

	if (git_ignore__for_path(repo, path, &ignores) < 0)
		return -1;

	error = git_ignore__lookup(&ignores, path, ignored);
	git_ignore__free(&ignores);
	return error;
}
