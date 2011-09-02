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

static int status_cmp(const void *a, const void *b)
{
	const struct status_entry *entry_a = (const struct status_entry *)(a);
	const struct status_entry *entry_b = (const struct status_entry *)(b);

	return strcmp(entry_a->path, entry_b->path);
}

static int status_srch(const void *key, const void *array_member)
{
	const char *path = (const char *)key;
	const struct status_entry *entry = (const struct status_entry *)(array_member);

	return strcmp(path, entry->path);
}

static int find_status_entry(git_vector *entries, const char *path)
{
	return git_vector_bsearch2(entries, status_srch, path);
}

static struct status_entry *new_status_entry(git_vector *entries, const char *path)
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

static void recurse_tree_entries(git_tree *tree, git_vector *entries, char *path)
{
	int i, cnt, idx;
	struct status_entry *e;
	char file_path[GIT_PATH_MAX];
	git_tree *subtree;

	cnt = git_tree_entrycount(tree);
	for (i = 0; i < cnt; ++i) {
		const git_tree_entry *tree_entry = git_tree_entry_byindex(tree, i);

		git_path_join(file_path, path, tree_entry->filename);

		if (git_tree_lookup(&subtree, tree->object.repo, &tree_entry->oid) == GIT_SUCCESS) {
			recurse_tree_entries(subtree, entries, file_path);
			git_tree_close(subtree);
			continue;
		}

		if ((idx = find_status_entry(entries, file_path)) != GIT_ENOTFOUND)
			e = (struct status_entry *)git_vector_get(entries, idx);
		else
			e = new_status_entry(entries, file_path);

		git_oid_cpy(&e->head_oid, &tree_entry->oid);
	}
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

		git_oid_cpy(&e->head_oid, &tree_entry->oid);
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
		return git__throw(GIT_EOBJCORRUPTED, "Can't find tree object '%s'", &tree_entry->filename);

	error = recurse_tree_entry(subtree, e, dir_sep+1);
	git_tree_close(subtree);
	return error;
}

struct status_st {
	union {
		git_vector *vector;
		struct status_entry *e;
	} entry;

	int workdir_path_len;
};

static int dirent_cb(void *state, char *full_path)
{
	int idx;
	struct status_entry *e;
	struct status_st *st = (struct status_st *)state;
	char *file_path = full_path + st->workdir_path_len;
	struct stat filest;
	git_oid oid;

	if ((git_futils_isdir(full_path) == GIT_SUCCESS) && (!strcmp(DOT_GIT, file_path)))
		return 0;

	if (git_futils_isdir(full_path) == GIT_SUCCESS)
		return git_futils_direach(full_path, GIT_PATH_MAX, dirent_cb, state);

	if ((idx = find_status_entry(st->entry.vector, file_path)) != GIT_ENOTFOUND) {
		e = (struct status_entry *)git_vector_get(st->entry.vector, idx);

		if (p_stat(full_path, &filest) < 0)
			return git__throw(GIT_EOSERR, "Failed to read file %s", full_path);

		if (e->mtime.seconds == (git_time_t)filest.st_mtime) {
			git_oid_cpy(&e->wt_oid, &e->index_oid);
			return 0;
		}
	} else {
		e = new_status_entry(st->entry.vector, file_path);
	}

	git_odb_hashfile(&oid, full_path, GIT_OBJ_BLOB);
	git_oid_cpy(&e->wt_oid, &oid);

	return 0;
}

static int set_status_flags(struct status_entry *e)
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

int git_status_foreach(git_repository *repo, int (*callback)(const char *, unsigned int, void *), void *payload)
{
	git_vector entries;
	struct status_entry *e;
	git_index *index;
	unsigned int i, cnt;
	git_index_entry *index_entry;
	char temp_path[GIT_PATH_MAX];
	int error;
	git_tree *tree;
	struct status_st dirent_st;

	git_reference *head_ref, *resolved_head_ref;
	git_commit *head_commit;

	git_repository_index(&index, repo);

	cnt = git_index_entrycount(index);
	git_vector_init(&entries, cnt, status_cmp);
	for (i = 0; i < cnt; ++i) {
		index_entry = git_index_get(index, i);

		e = new_status_entry(&entries, index_entry->path);
		git_oid_cpy(&e->index_oid, &index_entry->oid);
		e->mtime = index_entry->mtime;
	}
	git_index_free(index);

	git_reference_lookup(&head_ref, repo, GIT_HEAD_FILE);
	git_reference_resolve(&resolved_head_ref, head_ref);

	git_commit_lookup(&head_commit, repo, git_reference_oid(resolved_head_ref));

	// recurse through tree entries
	git_commit_tree(&tree, head_commit);
	recurse_tree_entries(tree, &entries, "");
	git_tree_close(tree);
	git_commit_close(head_commit);

	dirent_st.workdir_path_len = strlen(repo->path_workdir);
	dirent_st.entry.vector = &entries;
	strcpy(temp_path, repo->path_workdir);
	git_futils_direach(temp_path, GIT_PATH_MAX, dirent_cb, &dirent_st);

	for (i = 0; i < entries.length; ++i) {
		e = (struct status_entry *)git_vector_get(&entries, i);

		set_status_flags(e);
	}


	for (i = 0; i < entries.length; ++i) {
		e = (struct status_entry *)git_vector_get(&entries, i);

		if ((error = callback(e->path, e->status_flags, payload)) < GIT_SUCCESS)
			return git__throw(error, "Failed to list statuses. User callback failed");
	}

	for (i = 0; i < entries.length; ++i) {
		e = (struct status_entry *)git_vector_get(&entries, i);
		free(e);
	}
	git_vector_free(&entries);

	return GIT_SUCCESS;
}

static int retrieve_head_tree(git_tree **tree_out, git_repository *repo)
{
	git_reference *resolved_head_ref;
	git_commit *head_commit = NULL;
	git_tree *tree;
	int error = GIT_SUCCESS;

	*tree_out = NULL;

	error = git_repository_head(&resolved_head_ref, repo);
	if (error != GIT_SUCCESS && error != GIT_ENOTFOUND)
		return git__rethrow(error, "HEAD can't be resolved");

	/*
	 * We assume that a situation where HEAD exists but can not be resolved is valid.
	 * A new repository fits this description for instance.
	 */

	if (error == GIT_ENOTFOUND)
		return GIT_SUCCESS;

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

int git_status_file(unsigned int *status_flags, git_repository *repo, const char *path)
{
	struct status_entry *e;
	git_index *index = NULL;
	git_index_entry *index_entry;
	char temp_path[GIT_PATH_MAX];
	int idx, error = GIT_SUCCESS;
	git_tree *tree = NULL;
	struct stat filest;

	assert(status_flags && repo && path);

	git_path_join(temp_path, repo->path_workdir, path);
	if (git_futils_isdir(temp_path) == GIT_SUCCESS)
		return git__throw(GIT_EINVALIDPATH, "Failed to determine status of file '%s'. Provided path leads to a folder, not a file", path);

	e = new_status_entry(NULL, path);
	if (e == NULL)
		return GIT_ENOMEM;

	/* Find file in Workdir */
	if (git_futils_exists(temp_path) == GIT_SUCCESS) {
		if (p_stat(temp_path, &filest) < GIT_SUCCESS) {
			error = git__throw(GIT_EOSERR, "Failed to determine status of file '%s'. Can't read file", temp_path);
			goto exit;
		}

		if (e->mtime.seconds == (git_time_t)filest.st_mtime)
			git_oid_cpy(&e->wt_oid, &e->index_oid);
		else
			git_odb_hashfile(&e->wt_oid, temp_path, GIT_OBJ_BLOB);
	}

	/* Find file in Index */
	if ((error = git_repository_index(&index, repo)) < GIT_SUCCESS) {
		error = git__rethrow(error, "Failed to determine status of file '%s'. Index can't be opened", path);
		goto exit;
	}

	idx = git_index_find(index, path);
	if (idx >= 0) {
		index_entry = git_index_get(index, idx);
		git_oid_cpy(&e->index_oid, &index_entry->oid);
		e->mtime = index_entry->mtime;
	}
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
	if ((error = set_status_flags(e)) < GIT_SUCCESS) {
		error = git__throw(error, "Nonexistent file");
		goto exit;
	}

	*status_flags = e->status_flags;

exit:
	git_tree_close(tree);
	free(e);
	return error;
}
