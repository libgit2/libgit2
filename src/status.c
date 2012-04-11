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

#include "git2/diff.h"
#include "diff.h"

static int resolve_head_to_tree(git_tree **tree, git_repository *repo)
{
	git_reference *head = NULL;
	git_object *obj = NULL;

	if (git_reference_lookup(&head, repo, GIT_HEAD_FILE) < 0)
		return -1;

	if (git_reference_oid(head) == NULL) {
		git_reference *resolved;

		if (git_reference_resolve(&resolved, head) < 0) {
			/* cannot resolve HEAD - probably brand new repo */
			giterr_clear();
			git_reference_free(head);
			return GIT_ENOTFOUND;
		}

		git_reference_free(head);
		head = resolved;
	}

	if (git_object_lookup(&obj, repo, git_reference_oid(head), GIT_OBJ_ANY) < 0)
		goto fail;

	git_reference_free(head);

	switch (git_object_type(obj)) {
	case GIT_OBJ_TREE:
		*tree = (git_tree *)obj;
		break;
	case GIT_OBJ_COMMIT:
		if (git_commit_tree(tree, (git_commit *)obj) < 0)
			goto fail;
		git_object_free(obj);
		break;
	default:
		goto fail;
	}

	return 0;

fail:
	git_object_free(obj);
	git_reference_free(head);
	return -1;
}

static unsigned int index_delta2status(git_delta_t index_status)
{
	unsigned int st = GIT_STATUS_CURRENT;

	switch (index_status) {
	case GIT_DELTA_ADDED:
	case GIT_DELTA_COPIED:
	case GIT_DELTA_RENAMED:
		st = GIT_STATUS_INDEX_NEW;
		break;
	case GIT_DELTA_DELETED:
		st = GIT_STATUS_INDEX_DELETED;
		break;
	case GIT_DELTA_MODIFIED:
		st = GIT_STATUS_INDEX_MODIFIED;
		break;
	default:
		break;
	}

	return st;
}

static unsigned int workdir_delta2status(git_delta_t workdir_status)
{
	unsigned int st = GIT_STATUS_CURRENT;

	switch (workdir_status) {
	case GIT_DELTA_ADDED:
	case GIT_DELTA_COPIED:
	case GIT_DELTA_RENAMED:
	case GIT_DELTA_UNTRACKED:
		st = GIT_STATUS_WT_NEW;
		break;
	case GIT_DELTA_DELETED:
		st = GIT_STATUS_WT_DELETED;
		break;
	case GIT_DELTA_MODIFIED:
		st = GIT_STATUS_WT_MODIFIED;
		break;
	case GIT_DELTA_IGNORED:
		st = GIT_STATUS_IGNORED;
		break;
	default:
		break;
	}

	return st;
}

int git_status_foreach_ext(
	git_repository *repo,
	git_status_options *opts,
	int (*cb)(const char *, unsigned int, void *),
	void *cbdata)
{
	int err = 0, cmp;
	git_diff_options diffopt;
	git_diff_list *idx2head = NULL, *wd2idx = NULL;
	git_tree *head = NULL;
	git_status_show_t show =
		opts ? opts->show : GIT_STATUS_SHOW_INDEX_AND_WORKDIR;
	git_diff_delta *i2h, *w2i;
	unsigned int i, j, i_max, j_max;

	assert(show <= GIT_STATUS_SHOW_INDEX_THEN_WORKDIR);

	switch (resolve_head_to_tree(&head, repo)) {
	case 0: break;
	case GIT_ENOTFOUND: return 0;
	default: return -1;
	}

	memset(&diffopt, 0, sizeof(diffopt));
	memcpy(&diffopt.pathspec, &opts->pathspec, sizeof(diffopt.pathspec));

	if ((opts->flags & GIT_STATUS_OPT_INCLUDE_UNTRACKED) != 0)
		diffopt.flags = diffopt.flags | GIT_DIFF_INCLUDE_UNTRACKED;
	if ((opts->flags & GIT_STATUS_OPT_INCLUDE_IGNORED) != 0)
		diffopt.flags = diffopt.flags | GIT_DIFF_INCLUDE_IGNORED;
	if ((opts->flags & GIT_STATUS_OPT_INCLUDE_UNMODIFIED) != 0)
		diffopt.flags = diffopt.flags | GIT_DIFF_INCLUDE_UNMODIFIED;
	if ((opts->flags & GIT_STATUS_OPT_RECURSE_UNTRACKED_DIRS) != 0)
		diffopt.flags = diffopt.flags | GIT_DIFF_RECURSE_UNTRACKED_DIRS;
	/* TODO: support EXCLUDE_SUBMODULES flag */

	if (show != GIT_STATUS_SHOW_WORKDIR_ONLY &&
		(err = git_diff_index_to_tree(repo, &diffopt, head, &idx2head)) < 0)
		goto cleanup;

	if (show != GIT_STATUS_SHOW_INDEX_ONLY &&
		(err = git_diff_workdir_to_index(repo, &diffopt, &wd2idx)) < 0)
		goto cleanup;

	if (show == GIT_STATUS_SHOW_INDEX_THEN_WORKDIR) {
		for (i = 0; !err && i < idx2head->deltas.length; i++) {
			i2h = GIT_VECTOR_GET(&idx2head->deltas, i);
			err = cb(i2h->old.path, index_delta2status(i2h->status), cbdata);
		}
		git_diff_list_free(idx2head);
		idx2head = NULL;
	}

	i_max = idx2head ? idx2head->deltas.length : 0;
	j_max = wd2idx   ? wd2idx->deltas.length   : 0;

	for (i = 0, j = 0; !err && (i < i_max || j < j_max); ) {
		i2h = idx2head ? GIT_VECTOR_GET(&idx2head->deltas,i) : NULL;
		w2i = wd2idx   ? GIT_VECTOR_GET(&wd2idx->deltas,j)   : NULL;

		cmp = !w2i ? -1 : !i2h ? 1 : strcmp(i2h->old.path, w2i->old.path);

		if (cmp < 0) {
			err = cb(i2h->old.path, index_delta2status(i2h->status), cbdata);
			i++;
		} else if (cmp > 0) {
			err = cb(w2i->old.path, workdir_delta2status(w2i->status), cbdata);
			j++;
		} else {
			err = cb(i2h->old.path, index_delta2status(i2h->status) |
					 workdir_delta2status(w2i->status), cbdata);
			i++; j++;
		}
	}

cleanup:
	git_tree_free(head);
	git_diff_list_free(idx2head);
	git_diff_list_free(wd2idx);
	return err;
}

int git_status_foreach(
	git_repository *repo,
	int (*callback)(const char *, unsigned int, void *),
	void *payload)
{
	git_status_options opts;

	opts.show  = GIT_STATUS_SHOW_INDEX_AND_WORKDIR;
	opts.flags = GIT_STATUS_OPT_INCLUDE_IGNORED |
		GIT_STATUS_OPT_INCLUDE_UNTRACKED |
		GIT_STATUS_OPT_RECURSE_UNTRACKED_DIRS;

	return git_status_foreach_ext(repo, &opts, callback, payload);
}


/*
 * the old stuff
 */

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
	int is_dir:1;
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

static int dirent_cb(void *state, git_buf *full_path);
static int alphasorted_futils_direach(
	git_buf *path, int (*fn)(void *, git_buf *), void *arg);

static int process_folder(
	struct status_st *st,
	const git_tree_entry *tree_entry,
	git_buf *full_path,
	enum path_type path_type)
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

		case GIT_OBJ_COMMIT:
			/* TODO: proper submodule support */
			break;

		default:
			giterr_set(GITERR_REPOSITORY, "Unexpected tree entry type");
			return -1;
		}
	}


	if (full_path != NULL && path_type == GIT_STATUS_PATH_FOLDER) {
		git_ignores ignores, *old_ignores;

		if ((error = git_ignore__for_path(st->repo,
			full_path->ptr + st->workdir_path_len, &ignores)) == 0)
		{
			old_ignores = st->ignores;
			st->ignores = &ignores;

			error = alphasorted_futils_direach(full_path, dirent_cb, st);

			git_ignore__free(st->ignores);
			st->ignores = old_ignores;
		}
	} else {
		error = dirent_cb(st, NULL);
	}

	if (tree_entry_type == GIT_OBJ_TREE) {
		git_object_free(subtree);
		st->head_tree_relative_path_len -= 1 + tree_entry->filename_len;
		st->tree = pushed_tree;
		st->tree_position = pushed_tree_position;
		st->tree_position++;
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

static int determine_status(
	struct status_st *st,
	int in_head, int in_index, int in_workdir,
	const git_tree_entry *tree_entry,
	const git_index_entry *index_entry,
	git_buf *full_path,
	const char *status_path,
	enum path_type path_type)
{
	struct status_entry *e;
	int error = 0;
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

		if (in_workdir &&
			(error = status_entry_update_from_workdir(e, full_path->ptr)) < 0)
			return error;	/* The callee has already set the error message */

		return store_if_changed(st, e);
	}

	/* Are we dealing with a subtree? */
	if (tree_entry_type == GIT_OBJ_TREE) {
		assert(in_head && !in_index && !in_workdir);
		return process_folder(st, tree_entry, full_path, path_type);
	}

	/* We're dealing with something else -- most likely a submodule;
	 * skip it for now */
	if (in_head)
		st->tree_position++;
	if (in_index)
		st->index_position++;
	return 0;
}

static int path_type_from(git_buf *full_path, int is_dir)
{
	if (full_path == NULL)
		return GIT_STATUS_PATH_NULL;

	if (!is_dir)
		return GIT_STATUS_PATH_FILE;

	if (!git__suffixcmp(full_path->ptr, "/" DOT_GIT "/"))
		return GIT_STATUS_PATH_IGNORE;

	return GIT_STATUS_PATH_FOLDER;
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

/* Greatly inspired from JGit IndexTreeWalker */
/* https://github.com/spearce/jgit/blob/ed47e29c777accfa78c6f50685a5df2b8f5b8ff5/org.spearce.jgit/src/org/spearce/jgit/lib/IndexTreeWalker.java#L88 */

static int dirent_cb(void *state, git_buf *a)
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
		return 0;	/* Let's skip the ".git" directory */

	a_name = (path_type != GIT_STATUS_PATH_NULL) ? a->ptr + st->workdir_path_len : NULL;

	/* Loop over head tree and index up to and including this workdir file */
	while (1) {
		if (st->tree == NULL)
			m = NULL;
		else
			m = git_tree_entry_byindex(st->tree, st->tree_position);

		entry = git_index_get(st->index, st->index_position);

		if ((m == NULL) && (a == NULL) && (entry == NULL))
			return 0;

		if (m != NULL) {
			git_buf_truncate(&st->head_tree_relative_path,
							 st->head_tree_relative_path_len);
			git_buf_joinpath(&st->head_tree_relative_path,
							 st->head_tree_relative_path.ptr, m->filename);
			/* When the tree entry is a folder, append a forward slash to its name */
			if (git_tree_entry_type(m) == GIT_OBJ_TREE)
				git_path_to_dir(&st->head_tree_relative_path);

			if (git_buf_oom(&st->head_tree_relative_path))
				return -1;

			m_name = st->head_tree_relative_path.ptr;
		} else
			m_name = NULL;

		i_name = (entry != NULL) ? entry->path : NULL;

		cmpma = compare(m_name, a_name);
		cmpmi = compare(m_name, i_name);
		cmpai = compare(a_name, i_name);

		pm = ((cmpma <= 0) && (cmpmi <= 0)) ? m_name : NULL;
		pa = ((cmpma >= 0) && (cmpai <= 0)) ? a_name : NULL;
		pi = ((cmpmi >= 0) && (cmpai >= 0)) ? i_name : NULL;

		if ((error = determine_status(st, pm != NULL, pi != NULL, pa != NULL,
				m, entry, a, status_path(pm, pi, pa), path_type)) < 0)
			return error;

		if ((pa != NULL) || (path_type == GIT_STATUS_PATH_FOLDER))
			return 0;
	}
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
	git_buf *path,
	int (*fn)(void *, git_buf *),
	void *arg)
{
	int error;
	char *entry;
	git_vector entry_names;
	unsigned int idx;

	if (git_vector_init(&entry_names, 16, git__strcmp_cb) < 0)
		return -1;

	if ((error = git_path_dirload(path->ptr, 0, 1, &entry_names)) < 0)
		return error;

	git_vector_foreach(&entry_names, idx, entry) {
		size_t entry_len = strlen(entry);
		if (git_path_isdir(entry)) {
			/* dirload allocated 1 extra byte so there is space for slash */
			entry[entry_len++] = '/';
			entry[entry_len]   = '\0';
		}
	}

	git_vector_sort(&entry_names);

	git_vector_foreach(&entry_names, idx, entry) {
		/* Walk the entire vector even if there is an error, in order to
		 * free up memory, but stop making callbacks after an error.
		 */
		if (!error) {
			git_buf entry_path = GIT_BUF_INIT;
			git_buf_attach(&entry_path, entry, 0);

			((struct status_st *)arg)->is_dir =
				(entry_path.ptr[entry_path.size - 1] == '/');

			error = fn(arg, &entry_path);
		}

		git__free(entry);
	}

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

