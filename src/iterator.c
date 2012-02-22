/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "iterator.h"
#include "tree.h"
#include "ignore.h"
#include "buffer.h"

#define IDX_AS_PTR(I)		(void *)((uint64_t)(I))
#define PTR_AS_IDX(P)		(unsigned int)((uint64_t)(P))

typedef struct {
	git_iterator cb;
	git_repository *repo;
	git_vector tree_stack;
	git_vector idx_stack;
	git_index_entry entry;
	git_buf path;
} git_iterator_tree;

static const git_tree_entry *git_iterator__tree_entry(git_iterator_tree *ti)
{
	git_tree *tree;
	unsigned int tree_idx;

	if ((tree = git_vector_last(&ti->tree_stack)) == NULL)
		return NULL;

	tree_idx = PTR_AS_IDX(git_vector_last(&ti->idx_stack));
	return git_tree_entry_byindex(tree, tree_idx);
}

static int git_iterator__tree_current(
	git_iterator *self, const git_index_entry **entry)
{
	int error;
	git_iterator_tree *ti = (git_iterator_tree *)self;
	const git_tree_entry *te = git_iterator__tree_entry(ti);

	*entry = NULL;

	if (te == NULL)
		return GIT_SUCCESS;

	ti->entry.mode = te->attr;
	git_oid_cpy(&ti->entry.oid, &te->oid);
	error = git_buf_joinpath(&ti->path, ti->path.ptr, te->filename);
	if (error < GIT_SUCCESS)
		return error;
	ti->entry.path = ti->path.ptr;

	*entry = &ti->entry;

	return GIT_SUCCESS;
}

static int git_iterator__tree_at_end(git_iterator *self)
{
	git_iterator_tree *ti = (git_iterator_tree *)self;
	git_tree *tree;
	return ((tree = git_vector_last(&ti->tree_stack)) == NULL ||
			git_tree_entry_byindex(
				tree, PTR_AS_IDX(git_vector_last(&ti->idx_stack))) == NULL);
}

static int expand_tree_if_needed(git_iterator_tree *ti)
{
	int error;
	git_tree *tree, *subtree;
	unsigned int tree_idx;
	const git_tree_entry *te;

	while (1) {
		tree = git_vector_last(&ti->tree_stack);
		tree_idx = PTR_AS_IDX(git_vector_last(&ti->idx_stack));
		te = git_tree_entry_byindex(tree, tree_idx);

		if (!entry_is_tree(te))
			break;

		error = git_tree_lookup(&subtree, ti->repo, &te->oid);
		if (error != GIT_SUCCESS)
			return error;

		if ((error = git_vector_insert(&ti->tree_stack, subtree)) < GIT_SUCCESS ||
			(error = git_vector_insert(&ti->idx_stack, IDX_AS_PTR(0))) < GIT_SUCCESS ||
			(error = git_buf_joinpath(&ti->path, ti->path.ptr, te->filename)) < GIT_SUCCESS)
		{
			git_tree_free(subtree);
			return error;
		}
	}

	return GIT_SUCCESS;
}

static int git_iterator__tree_advance(
	git_iterator *self, const git_index_entry **entry)
{
	int error = GIT_SUCCESS;
	git_iterator_tree *ti = (git_iterator_tree *)self;
	git_tree *tree = git_vector_last(&ti->tree_stack);
	unsigned int tree_idx = PTR_AS_IDX(git_vector_last(&ti->idx_stack));
	const git_tree_entry *te = git_tree_entry_byindex(tree, tree_idx);

	if (entry != NULL)
		*entry = NULL;

	if (te == NULL)
		return GIT_SUCCESS;

	while (1) {
		/* advance this tree */
		tree_idx++;
		ti->idx_stack.contents[ti->idx_stack.length - 1] = IDX_AS_PTR(tree_idx);

		/* remove old entry filename */
		git_buf_rtruncate_at_char(&ti->path, '/');

		if ((te = git_tree_entry_byindex(tree, tree_idx)) != NULL)
			break;

		/* no entry - either we are done or we are done with this subtree */
		if (ti->tree_stack.length == 1)
			return GIT_SUCCESS;

		git_tree_free(tree);
		git_vector_remove(&ti->tree_stack, ti->tree_stack.length - 1);
		git_vector_remove(&ti->idx_stack, ti->idx_stack.length - 1);
		git_buf_rtruncate_at_char(&ti->path, '/');

		tree = git_vector_last(&ti->tree_stack);
		tree_idx = PTR_AS_IDX(git_vector_last(&ti->idx_stack));
	}

	if (te && entry_is_tree(te))
		error = expand_tree_if_needed(ti);

	if (error == GIT_SUCCESS && entry != NULL)
		error = git_iterator__tree_current(self, entry);

	return error;
}

static void git_iterator__tree_free(git_iterator *self)
{
	git_iterator_tree *ti = (git_iterator_tree *)self;

	while (ti->tree_stack.length > 1) {
		git_tree *tree = git_vector_last(&ti->tree_stack);
		git_tree_free(tree);
		git_vector_remove(&ti->tree_stack, ti->tree_stack.length - 1);
	}

	git_vector_clear(&ti->tree_stack);
	git_vector_clear(&ti->idx_stack);
	git_buf_free(&ti->path);
}

int git_iterator_for_tree(git_repository *repo, git_tree *tree, git_iterator **iter)
{
	int error;
	git_iterator_tree *ti = git__calloc(1, sizeof(git_iterator_tree));
	if (!ti)
		return GIT_ENOMEM;

	ti->cb.type    = GIT_ITERATOR_TREE;
	ti->cb.current = git_iterator__tree_current;
	ti->cb.at_end  = git_iterator__tree_at_end;
	ti->cb.advance = git_iterator__tree_advance;
	ti->cb.free    = git_iterator__tree_free;
	ti->repo       = repo;

	if (!(error = git_vector_init(&ti->tree_stack, 0, NULL)) &&
		!(error = git_vector_insert(&ti->tree_stack, tree)) &&
		!(error = git_vector_init(&ti->idx_stack, 0, NULL)))
		error   = git_vector_insert(&ti->idx_stack, IDX_AS_PTR(0));

	if (error == GIT_SUCCESS)
		error = expand_tree_if_needed(ti);

	if (error != GIT_SUCCESS)
		git_iterator_free((git_iterator *)ti);
	else
		*iter = (git_iterator *)ti;

	return error;
}


typedef struct {
	git_iterator cb;
	git_index *index;
	unsigned int current;
} git_iterator_index;

static int git_iterator__index_current(
	git_iterator *self, const git_index_entry **entry)
{
	git_iterator_index *ii = (git_iterator_index *)self;
	*entry = git_index_get(ii->index, ii->current);
	return GIT_SUCCESS;
}

static int git_iterator__index_at_end(git_iterator *self)
{
	git_iterator_index *ii = (git_iterator_index *)self;
	return (ii->current >= git_index_entrycount(ii->index));
}

static int git_iterator__index_advance(
	git_iterator *self, const git_index_entry **entry)
{
	git_iterator_index *ii = (git_iterator_index *)self;
	if (ii->current < git_index_entrycount(ii->index))
		ii->current++;
	if (entry)
		*entry = git_index_get(ii->index, ii->current);
	return GIT_SUCCESS;
}

static void git_iterator__index_free(git_iterator *self)
{
	git_iterator_index *ii = (git_iterator_index *)self;
	git_index_free(ii->index);
	ii->index = NULL;
}

int git_iterator_for_index(git_repository *repo, git_iterator **iter)
{
	int error;
	git_iterator_index *ii = git__calloc(1, sizeof(git_iterator_index));
	if (!ii)
		return GIT_ENOMEM;

	ii->cb.type    = GIT_ITERATOR_INDEX;
	ii->cb.current = git_iterator__index_current;
	ii->cb.at_end  = git_iterator__index_at_end;
	ii->cb.advance = git_iterator__index_advance;
	ii->cb.free    = git_iterator__index_free;
	ii->current    = 0;

	if ((error = git_repository_index(&ii->index, repo)) < GIT_SUCCESS)
		git__free(ii);
	else
		*iter = (git_iterator *)ii;
	return error;
}


typedef struct {
	git_iterator cb;
	git_repository *repo;
	size_t root_len;
	git_vector dir_stack; /* vector of vectors of paths */
	git_vector idx_stack;
	git_ignores ignores;
	git_index_entry entry;
	git_buf path;
	int is_ignored;
} git_iterator_workdir;

static void free_directory(git_vector *dir)
{
	unsigned int i;
	char *path;

	git_vector_foreach(dir, i, path)
		git__free(path);
	git_vector_free(dir);
	git__free(dir);
}

static int load_workdir_entry(git_iterator_workdir *wi);

static int push_directory(git_iterator_workdir *wi)
{
	int error;
	git_vector *dir = NULL;

	error = git_vector_alloc(&dir, 0, git__strcmp_cb);
	if (error < GIT_SUCCESS)
		return error;

	/* allocate dir entries with extra byte (the "1" param) so later on we
	 * can suffix directories with a "/" as needed.
	 */
	error = git_path_dirload(wi->path.ptr, wi->root_len, 1, dir);
	if (error < GIT_SUCCESS || dir->length == 0) {
		free_directory(dir);
		return GIT_ENOTFOUND;
	}

	if ((error = git_vector_insert(&wi->dir_stack, dir)) ||
		(error = git_vector_insert(&wi->idx_stack, IDX_AS_PTR(0))))
	{
		free_directory(dir);
		return error;
	}

	git_vector_sort(dir);

	if (wi->dir_stack.length > 1) {
		int slash_pos = git_buf_rfind_next(&wi->path, '/');
		(void)git_ignore__push_dir(&wi->ignores, &wi->path.ptr[slash_pos + 1]);
	}

	return load_workdir_entry(wi);
}

static int git_iterator__workdir_current(
	git_iterator *self, const git_index_entry **entry)
{
	git_iterator_workdir *wi = (git_iterator_workdir *)self;
	*entry = (wi->entry.path == NULL) ? NULL : &wi->entry;
	return GIT_SUCCESS;
}

static int git_iterator__workdir_at_end(git_iterator *self)
{
	git_iterator_workdir *wi = (git_iterator_workdir *)self;
	return (wi->entry.path == NULL);
}

static int git_iterator__workdir_advance(
	git_iterator *self, const git_index_entry **entry)
{
	int error;
	git_iterator_workdir *wi = (git_iterator_workdir *)self;
	git_vector *dir;
	unsigned int pos;
	const char *next;

	if (entry)
		*entry = NULL;

	if (wi->entry.path == NULL)
		return GIT_SUCCESS;

	while (1) {
		dir = git_vector_last(&wi->dir_stack);
		pos = 1 + PTR_AS_IDX(git_vector_last(&wi->idx_stack));
		wi->idx_stack.contents[wi->idx_stack.length - 1] = IDX_AS_PTR(pos);

		next = git_vector_get(dir, pos);
		if (next != NULL) {
			if (strcmp(next, DOT_GIT) == 0)
				continue;
			/* else found a good entry */
			break;
		}

		memset(&wi->entry, 0, sizeof(wi->entry));
		if (wi->dir_stack.length == 1)
			return GIT_SUCCESS;

		free_directory(dir);
		git_vector_remove(&wi->dir_stack, wi->dir_stack.length - 1);
		git_vector_remove(&wi->idx_stack, wi->idx_stack.length - 1);
		git_ignore__pop_dir(&wi->ignores);
	}

	error = load_workdir_entry(wi);

	if (error == GIT_SUCCESS && entry)
		return git_iterator__workdir_current(self, entry);

	return error;
}

int git_iterator_advance_into_directory(
	git_iterator *iter, const git_index_entry **entry)
{
	git_iterator_workdir *wi = (git_iterator_workdir *)iter;

	if (iter->type != GIT_ITERATOR_WORKDIR)
		return git_iterator_current(iter, entry);

	/* Loop because the first entry in the ignored directory could itself be
	 * an ignored directory, but we want to descend to find an actual entry.
	 */
	if (wi->entry.path && S_ISDIR(wi->entry.mode)) {
		if (push_directory(wi) < GIT_SUCCESS)
			/* If error loading or if empty, skip the directory. */
			return git_iterator__workdir_advance((git_iterator *)wi, entry);
	}

	return git_iterator__workdir_current(iter, entry);
}

static void git_iterator__workdir_free(git_iterator *self)
{
	git_iterator_workdir *wi = (git_iterator_workdir *)self;

	while (wi->dir_stack.length) {
		git_vector *dir = git_vector_last(&wi->dir_stack);
		free_directory(dir);
		git_vector_remove(&wi->dir_stack, wi->dir_stack.length - 1);
	}

	git_vector_clear(&wi->dir_stack);
	git_vector_clear(&wi->idx_stack);
	git_ignore__free(&wi->ignores);
	git_buf_free(&wi->path);
}

static int load_workdir_entry(git_iterator_workdir *wi)
{
	int error;
	char *relpath;
	git_vector *dir = git_vector_last(&wi->dir_stack);
	unsigned int pos = PTR_AS_IDX(git_vector_last(&wi->idx_stack));
	struct stat st;

	relpath = git_vector_get(dir, pos);
	error = git_buf_joinpath(
		&wi->path, git_repository_workdir(wi->repo), relpath);
	if (error < GIT_SUCCESS)
		return error;

	memset(&wi->entry, 0, sizeof(wi->entry));
	wi->entry.path = relpath;

	if (strcmp(relpath, DOT_GIT) == 0)
		return git_iterator__workdir_advance((git_iterator *)wi, NULL);

	/* if there is an error processing the entry, treat as ignored */
	wi->is_ignored = 1;
	error = git_ignore__lookup(&wi->ignores, wi->entry.path, &wi->is_ignored);
	if (error != GIT_SUCCESS)
		return GIT_SUCCESS;

	if (p_lstat(wi->path.ptr, &st) < 0)
		return GIT_SUCCESS;

	/* TODO: remove shared code for struct stat conversion with index.c */
	wi->entry.ctime.seconds = (git_time_t)st.st_ctime;
	wi->entry.mtime.seconds = (git_time_t)st.st_mtime;
	wi->entry.dev  = st.st_rdev;
	wi->entry.ino  = st.st_ino;
	wi->entry.mode = git_futils_canonical_mode(st.st_mode);
	wi->entry.uid  = st.st_uid;
	wi->entry.gid  = st.st_gid;
	wi->entry.file_size = st.st_size;

	/* if this is a file type we don't handle, treat as ignored */
	if (st.st_mode == 0)
		return GIT_SUCCESS;

	if (S_ISDIR(st.st_mode)) {
		if (git_path_contains(&wi->path, DOT_GIT) == GIT_SUCCESS) {
			/* create submodule entry */
			wi->entry.mode = S_IFGITLINK;
		} else {
			/* create directory entry that can be advanced into as needed */
			size_t pathlen = strlen(wi->entry.path);
			wi->entry.path[pathlen] = '/';
			wi->entry.path[pathlen + 1] = '\0';
			wi->entry.mode = S_IFDIR;
		}
	}

	return GIT_SUCCESS;
}

int git_iterator_for_workdir(git_repository *repo, git_iterator **iter)
{
	int error;
	git_iterator_workdir *wi = git__calloc(1, sizeof(git_iterator_workdir));
	if (!wi)
		return GIT_ENOMEM;

	wi->cb.type    = GIT_ITERATOR_WORKDIR;
	wi->cb.current = git_iterator__workdir_current;
	wi->cb.at_end  = git_iterator__workdir_at_end;
	wi->cb.advance = git_iterator__workdir_advance;
	wi->cb.free    = git_iterator__workdir_free;
	wi->repo       = repo;

	if ((error = git_buf_sets(
			&wi->path, git_repository_workdir(repo))) < GIT_SUCCESS ||
		(error = git_vector_init(&wi->dir_stack, 0, NULL)) < GIT_SUCCESS ||
		(error = git_vector_init(&wi->idx_stack, 0, NULL)) < GIT_SUCCESS ||
		(error = git_ignore__for_path(repo, "", &wi->ignores)) < GIT_SUCCESS)
	{
		git__free(wi);
		return error;
	}

	wi->root_len = wi->path.size;

	if ((error = push_directory(wi)) < GIT_SUCCESS)
		git_iterator_free((git_iterator *)wi);
	else
		*iter = (git_iterator *)wi;

	return error;
}

int git_iterator_current_tree_entry(
	git_iterator *iter, const git_tree_entry **tree_entry)
{
	if (iter->type != GIT_ITERATOR_TREE)
		*tree_entry = NULL;
	else
		*tree_entry = git_iterator__tree_entry((git_iterator_tree *)iter);

	return GIT_SUCCESS;
}

int git_iterator_current_is_ignored(git_iterator *iter)
{
	if (iter->type != GIT_ITERATOR_WORKDIR)
		return 0;
	else
		return ((git_iterator_workdir *)iter)->is_ignored;
}
