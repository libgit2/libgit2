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
#include "git2/submodule.h"

#define ITERATOR_BASE_INIT(P,NAME_LC,NAME_UC) do { \
	(P) = git__calloc(1, sizeof(NAME_LC ## _iterator)); \
	GITERR_CHECK_ALLOC(P); \
	(P)->base.type    = GIT_ITERATOR_ ## NAME_UC; \
	(P)->base.start   = start ? git__strdup(start) : NULL; \
	(P)->base.end     = end ? git__strdup(end) : NULL; \
	(P)->base.current = NAME_LC ## _iterator__current; \
	(P)->base.at_end  = NAME_LC ## _iterator__at_end; \
	(P)->base.advance = NAME_LC ## _iterator__advance; \
	(P)->base.seek    = NAME_LC ## _iterator__seek; \
	(P)->base.reset   = NAME_LC ## _iterator__reset; \
	(P)->base.free    = NAME_LC ## _iterator__free; \
	if ((start && !(P)->base.start) || (end && !(P)->base.end)) \
		return -1; \
	} while (0)


static int empty_iterator__no_item(
	git_iterator *iter, const git_index_entry **entry)
{
	GIT_UNUSED(iter);
	*entry = NULL;
	return 0;
}

static int empty_iterator__at_end(git_iterator *iter)
{
	GIT_UNUSED(iter);
	return 1;
}

static int empty_iterator__noop(git_iterator *iter)
{
	GIT_UNUSED(iter);
	return 0;
}

static int empty_iterator__seek(git_iterator *iter, const char *prefix)
{
	GIT_UNUSED(iter);
	GIT_UNUSED(prefix);
	return -1;
}

static void empty_iterator__free(git_iterator *iter)
{
	GIT_UNUSED(iter);
}

int git_iterator_for_nothing(git_iterator **iter)
{
	git_iterator *i = git__calloc(1, sizeof(git_iterator));
	GITERR_CHECK_ALLOC(i);

	i->type    = GIT_ITERATOR_EMPTY;
	i->current = empty_iterator__no_item;
	i->at_end  = empty_iterator__at_end;
	i->advance = empty_iterator__no_item;
	i->seek    = empty_iterator__seek;
	i->reset   = empty_iterator__noop;
	i->free    = empty_iterator__free;

	*iter = i;

	return 0;
}


typedef struct tree_iterator_frame tree_iterator_frame;
struct tree_iterator_frame {
	tree_iterator_frame *next;
	git_tree *tree;
	char *start;
	unsigned int index;
};

typedef struct {
	git_iterator base;
	git_repository *repo;
	tree_iterator_frame *stack;
	git_index_entry entry;
	git_buf path;
	bool path_has_filename;
} tree_iterator;

static const git_tree_entry *tree_iterator__tree_entry(tree_iterator *ti)
{
	return (ti->stack == NULL) ? NULL :
		git_tree_entry_byindex(ti->stack->tree, ti->stack->index);
}

static char *tree_iterator__current_filename(
	tree_iterator *ti, const git_tree_entry *te)
{
	if (!ti->path_has_filename) {
		if (git_buf_joinpath(&ti->path, ti->path.ptr, te->filename) < 0)
			return NULL;
		ti->path_has_filename = true;
	}

	return ti->path.ptr;
}

static void tree_iterator__pop_frame(tree_iterator *ti)
{
	tree_iterator_frame *tf = ti->stack;
	ti->stack = tf->next;
	if (ti->stack != NULL) /* don't free the initial tree */
		git_tree_free(tf->tree);
	git__free(tf);
}

static int tree_iterator__to_end(tree_iterator *ti)
{
	while (ti->stack && ti->stack->next)
		tree_iterator__pop_frame(ti);

	if (ti->stack)
		ti->stack->index = git_tree_entrycount(ti->stack->tree);

	return 0;
}

static int tree_iterator__current(
	git_iterator *self, const git_index_entry **entry)
{
	tree_iterator *ti = (tree_iterator *)self;
	const git_tree_entry *te = tree_iterator__tree_entry(ti);

	if (entry)
		*entry = NULL;

	if (te == NULL)
		return 0;

	ti->entry.mode = te->attr;
	git_oid_cpy(&ti->entry.oid, &te->oid);

	ti->entry.path = tree_iterator__current_filename(ti, te);
	if (ti->entry.path == NULL)
		return -1;

	if (ti->base.end && git__prefixcmp(ti->entry.path, ti->base.end) > 0)
		return tree_iterator__to_end(ti);

	if (entry)
		*entry = &ti->entry;

	return 0;
}

static int tree_iterator__at_end(git_iterator *self)
{
	return (tree_iterator__tree_entry((tree_iterator *)self) == NULL);
}

static tree_iterator_frame *tree_iterator__alloc_frame(
	git_tree *tree, char *start)
{
	tree_iterator_frame *tf = git__calloc(1, sizeof(tree_iterator_frame));
	if (!tf)
		return NULL;

	tf->tree = tree;

	if (start && *start) {
		tf->start = start;
		tf->index = git_tree__prefix_position(tree, start);
	}

	return tf;
}

static int tree_iterator__expand_tree(tree_iterator *ti)
{
	int error;
	git_tree *subtree;
	const git_tree_entry *te = tree_iterator__tree_entry(ti);
	tree_iterator_frame *tf;
	char *relpath;

	while (te != NULL && git_tree_entry__is_tree(te)) {
		if (git_buf_joinpath(&ti->path, ti->path.ptr, te->filename) < 0)
			return -1;

		/* check that we have not passed the range end */
		if (ti->base.end != NULL &&
			git__prefixcmp(ti->path.ptr, ti->base.end) > 0)
			return tree_iterator__to_end(ti);

		if ((error = git_tree_lookup(&subtree, ti->repo, &te->oid)) < 0)
			return error;

		relpath = NULL;

		/* apply range start to new frame if relevant */
		if (ti->stack->start &&
			git__prefixcmp(ti->stack->start, te->filename) == 0)
		{
			size_t namelen = strlen(te->filename);
			if (ti->stack->start[namelen] == '/')
				relpath = ti->stack->start + namelen + 1;
		}

		if ((tf = tree_iterator__alloc_frame(subtree, relpath)) == NULL)
			return -1;

		tf->next  = ti->stack;
		ti->stack = tf;

		te = tree_iterator__tree_entry(ti);
	}

	return 0;
}

static int tree_iterator__advance(
	git_iterator *self, const git_index_entry **entry)
{
	int error = 0;
	tree_iterator *ti = (tree_iterator *)self;
	const git_tree_entry *te = NULL;

	if (entry != NULL)
		*entry = NULL;

	if (ti->path_has_filename) {
		git_buf_rtruncate_at_char(&ti->path, '/');
		ti->path_has_filename = false;
	}

	while (ti->stack != NULL) {
		te = git_tree_entry_byindex(ti->stack->tree, ++ti->stack->index);
		if (te != NULL)
			break;

		tree_iterator__pop_frame(ti);

		git_buf_rtruncate_at_char(&ti->path, '/');
	}

	if (te && git_tree_entry__is_tree(te))
		error = tree_iterator__expand_tree(ti);

	if (!error)
		error = tree_iterator__current(self, entry);

	return error;
}

static int tree_iterator__seek(git_iterator *self, const char *prefix)
{
	GIT_UNUSED(self);
	GIT_UNUSED(prefix);
	/* pop stack until matches prefix */
	/* seek item in current frame matching prefix */
	/* push stack which matches prefix */
	return -1;
}

static void tree_iterator__free(git_iterator *self)
{
	tree_iterator *ti = (tree_iterator *)self;
	while (ti->stack != NULL)
		tree_iterator__pop_frame(ti);
	git_buf_free(&ti->path);
}

static int tree_iterator__reset(git_iterator *self)
{
	tree_iterator *ti = (tree_iterator *)self;

	while (ti->stack && ti->stack->next)
		tree_iterator__pop_frame(ti);

	if (ti->stack)
		ti->stack->index =
			git_tree__prefix_position(ti->stack->tree, ti->base.start);

	git_buf_clear(&ti->path);

	return tree_iterator__expand_tree(ti);
}

int git_iterator_for_tree_range(
	git_iterator **iter,
	git_repository *repo,
	git_tree *tree,
	const char *start,
	const char *end)
{
	int error;
	tree_iterator *ti;

	if (tree == NULL)
		return git_iterator_for_nothing(iter);

	ITERATOR_BASE_INIT(ti, tree, TREE);

	ti->repo  = repo;
	ti->stack = tree_iterator__alloc_frame(tree, ti->base.start);

	if ((error = tree_iterator__expand_tree(ti)) < 0)
		git_iterator_free((git_iterator *)ti);
	else
		*iter = (git_iterator *)ti;

	return error;
}


typedef struct {
	git_iterator base;
	git_index *index;
	unsigned int current;
} index_iterator;

static int index_iterator__current(
	git_iterator *self, const git_index_entry **entry)
{
	index_iterator *ii = (index_iterator *)self;
	git_index_entry *ie = git_index_get(ii->index, ii->current);

	if (ie != NULL &&
		ii->base.end != NULL &&
		git__prefixcmp(ie->path, ii->base.end) > 0)
	{
		ii->current = git_index_entrycount(ii->index);
		ie = NULL;
	}

	if (entry)
		*entry = ie;

	return 0;
}

static int index_iterator__at_end(git_iterator *self)
{
	index_iterator *ii = (index_iterator *)self;
	return (ii->current >= git_index_entrycount(ii->index));
}

static int index_iterator__advance(
	git_iterator *self, const git_index_entry **entry)
{
	index_iterator *ii = (index_iterator *)self;

	if (ii->current < git_index_entrycount(ii->index))
		ii->current++;

	return index_iterator__current(self, entry);
}

static int index_iterator__seek(git_iterator *self, const char *prefix)
{
	GIT_UNUSED(self);
	GIT_UNUSED(prefix);
	/* find last item before prefix */
	return -1;
}

static int index_iterator__reset(git_iterator *self)
{
	index_iterator *ii = (index_iterator *)self;
	ii->current = 0;
	return 0;
}

static void index_iterator__free(git_iterator *self)
{
	index_iterator *ii = (index_iterator *)self;
	git_index_free(ii->index);
	ii->index = NULL;
}

int git_iterator_for_index_range(
	git_iterator **iter,
	git_repository *repo,
	const char *start,
	const char *end)
{
	int error;
	index_iterator *ii;

	ITERATOR_BASE_INIT(ii, index, INDEX);

	if ((error = git_repository_index(&ii->index, repo)) < 0)
		git__free(ii);
	else {
		ii->current = start ? git_index__prefix_position(ii->index, start) : 0;
		*iter = (git_iterator *)ii;
	}

	return error;
}


typedef struct workdir_iterator_frame workdir_iterator_frame;
struct workdir_iterator_frame {
	workdir_iterator_frame *next;
	git_vector entries;
	unsigned int index;
	char *start;
};

typedef struct {
	git_iterator base;
	git_repository *repo;
	size_t root_len;
	workdir_iterator_frame *stack;
	git_ignores ignores;
	git_index_entry entry;
	git_buf path;
	int is_ignored;
} workdir_iterator;

static workdir_iterator_frame *workdir_iterator__alloc_frame(void)
{
	workdir_iterator_frame *wf = git__calloc(1, sizeof(workdir_iterator_frame));
	if (wf == NULL)
		return NULL;
	if (git_vector_init(&wf->entries, 0, git_path_with_stat_cmp) != 0) {
		git__free(wf);
		return NULL;
	}
	return wf;
}

static void workdir_iterator__free_frame(workdir_iterator_frame *wf)
{
	unsigned int i;
	git_path_with_stat *path;

	git_vector_foreach(&wf->entries, i, path)
		git__free(path);
	git_vector_free(&wf->entries);
	git__free(wf);
}

static int workdir_iterator__update_entry(workdir_iterator *wi);

static int workdir_iterator__entry_cmp(const void *prefix, const void *item)
{
	const git_path_with_stat *ps = item;
	return git__prefixcmp((const char *)prefix, ps->path);
}

static int workdir_iterator__expand_dir(workdir_iterator *wi)
{
	int error;
	workdir_iterator_frame *wf = workdir_iterator__alloc_frame();
	GITERR_CHECK_ALLOC(wf);

	error = git_path_dirload_with_stat(wi->path.ptr, wi->root_len, &wf->entries);
	if (error < 0 || wf->entries.length == 0) {
		workdir_iterator__free_frame(wf);
		return GIT_ENOTFOUND;
	}

	git_vector_sort(&wf->entries);

	if (!wi->stack)
		wf->start = wi->base.start;
	else if (wi->stack->start &&
		git__prefixcmp(wi->stack->start, wi->path.ptr + wi->root_len) == 0)
		wf->start = wi->stack->start;

	if (wf->start)
		git_vector_bsearch3(
			&wf->index, &wf->entries, workdir_iterator__entry_cmp, wf->start);

	wf->next  = wi->stack;
	wi->stack = wf;

	/* only push new ignores if this is not top level directory */
	if (wi->stack->next != NULL) {
		ssize_t slash_pos = git_buf_rfind_next(&wi->path, '/');
		(void)git_ignore__push_dir(&wi->ignores, &wi->path.ptr[slash_pos + 1]);
	}

	return workdir_iterator__update_entry(wi);
}

static int workdir_iterator__current(
	git_iterator *self, const git_index_entry **entry)
{
	workdir_iterator *wi = (workdir_iterator *)self;
	*entry = (wi->entry.path == NULL) ? NULL : &wi->entry;
	return 0;
}

static int workdir_iterator__at_end(git_iterator *self)
{
	return (((workdir_iterator *)self)->entry.path == NULL);
}

static int workdir_iterator__advance(
	git_iterator *self, const git_index_entry **entry)
{
	int error;
	workdir_iterator *wi = (workdir_iterator *)self;
	workdir_iterator_frame *wf;
	git_path_with_stat *next;

	if (entry != NULL)
		*entry = NULL;

	if (wi->entry.path == NULL)
		return 0;

	while ((wf = wi->stack) != NULL) {
		next = git_vector_get(&wf->entries, ++wf->index);
		if (next != NULL) {
			if (strcmp(next->path, DOT_GIT "/") == 0)
				continue;
			/* else found a good entry */
			break;
		}

		/* pop workdir directory stack */
		wi->stack = wf->next;
		workdir_iterator__free_frame(wf);
		git_ignore__pop_dir(&wi->ignores);

		if (wi->stack == NULL) {
			memset(&wi->entry, 0, sizeof(wi->entry));
			return 0;
		}
	}

	error = workdir_iterator__update_entry(wi);

	if (!error && entry != NULL)
		error = workdir_iterator__current(self, entry);

	return error;
}

static int workdir_iterator__seek(git_iterator *self, const char *prefix)
{
	GIT_UNUSED(self);
	GIT_UNUSED(prefix);
	/* pop stack until matching prefix */
	/* find prefix item in current frame */
	/* push subdirectories as deep as possible while matching */
	return 0;
}

static int workdir_iterator__reset(git_iterator *self)
{
	workdir_iterator *wi = (workdir_iterator *)self;
	while (wi->stack != NULL && wi->stack->next != NULL) {
		workdir_iterator_frame *wf = wi->stack;
		wi->stack = wf->next;
		workdir_iterator__free_frame(wf);
		git_ignore__pop_dir(&wi->ignores);
	}
	if (wi->stack)
		wi->stack->index = 0;
	return 0;
}

static void workdir_iterator__free(git_iterator *self)
{
	workdir_iterator *wi = (workdir_iterator *)self;

	while (wi->stack != NULL) {
		workdir_iterator_frame *wf = wi->stack;
		wi->stack = wf->next;
		workdir_iterator__free_frame(wf);
	}

	git_ignore__free(&wi->ignores);
	git_buf_free(&wi->path);
}

static int workdir_iterator__update_entry(workdir_iterator *wi)
{
	git_path_with_stat *ps = git_vector_get(&wi->stack->entries, wi->stack->index);

	git_buf_truncate(&wi->path, wi->root_len);
	memset(&wi->entry, 0, sizeof(wi->entry));

	if (!ps)
		return 0;

	if (git_buf_put(&wi->path, ps->path, ps->path_len) < 0)
		return -1;

	if (wi->base.end &&
		git__prefixcmp(wi->path.ptr + wi->root_len, wi->base.end) > 0)
		return 0;

	wi->entry.path = ps->path;

	/* skip over .git directory */
	if (strcmp(ps->path, DOT_GIT "/") == 0)
		return workdir_iterator__advance((git_iterator *)wi, NULL);

	/* if there is an error processing the entry, treat as ignored */
	wi->is_ignored = 1;

	git_index__init_entry_from_stat(&ps->st, &wi->entry);

	/* need different mode here to keep directories during iteration */
	wi->entry.mode = git_futils_canonical_mode(ps->st.st_mode);

	/* if this is a file type we don't handle, treat as ignored */
	if (wi->entry.mode == 0)
		return 0;

	/* okay, we are far enough along to look up real ignore rule */
	if (git_ignore__lookup(&wi->ignores, wi->entry.path, &wi->is_ignored) < 0)
		return 0; /* if error, ignore it and ignore file */

	/* detect submodules */
	if (S_ISDIR(wi->entry.mode)) {
		bool is_submodule = git_path_contains(&wi->path, DOT_GIT);

		/* if there is no .git, still check submodules data */
		if (!is_submodule) {
			int res = git_submodule_lookup(NULL, wi->repo, wi->entry.path);
			is_submodule = (res == 0);
			if (res == GIT_ENOTFOUND)
				giterr_clear();
		}

		/* if submodule, mark as GITLINK and remove trailing slash */
		if (is_submodule) {
			size_t len = strlen(wi->entry.path);
			assert(wi->entry.path[len - 1] == '/');
			wi->entry.path[len - 1] = '\0';
			wi->entry.mode = S_IFGITLINK;
		}
	}

	return 0;
}

int git_iterator_for_workdir_range(
	git_iterator **iter,
	git_repository *repo,
	const char *start,
	const char *end)
{
	int error;
	workdir_iterator *wi;

	assert(iter && repo);

	if (git_repository_is_bare(repo)) {
		giterr_set(GITERR_INVALID,
			"Cannot scan working directory for bare repo");
		return -1;
	}

	ITERATOR_BASE_INIT(wi, workdir, WORKDIR);

	wi->repo = repo;

	if (git_buf_sets(&wi->path, git_repository_workdir(repo)) < 0 ||
		git_path_to_dir(&wi->path) < 0 ||
		git_ignore__for_path(repo, "", &wi->ignores) < 0)
	{
		git__free(wi);
		return -1;
	}

	wi->root_len = wi->path.size;

	if ((error = workdir_iterator__expand_dir(wi)) < 0) {
		if (error == GIT_ENOTFOUND)
			error = 0;
		else {
			git_iterator_free((git_iterator *)wi);
			wi = NULL;
		}
	}

	*iter = (git_iterator *)wi;

	return error;
}


int git_iterator_current_tree_entry(
	git_iterator *iter, const git_tree_entry **tree_entry)
{
	*tree_entry = (iter->type != GIT_ITERATOR_TREE) ? NULL :
		tree_iterator__tree_entry((tree_iterator *)iter);
	return 0;
}

int git_iterator_current_is_ignored(git_iterator *iter)
{
	return (iter->type != GIT_ITERATOR_WORKDIR) ? 0 :
		((workdir_iterator *)iter)->is_ignored;
}

int git_iterator_advance_into_directory(
	git_iterator *iter, const git_index_entry **entry)
{
	workdir_iterator *wi = (workdir_iterator *)iter;

	if (iter->type == GIT_ITERATOR_WORKDIR &&
		wi->entry.path &&
		S_ISDIR(wi->entry.mode) &&
		!S_ISGITLINK(wi->entry.mode))
	{
		if (workdir_iterator__expand_dir(wi) < 0)
			/* if error loading or if empty, skip the directory. */
			return workdir_iterator__advance(iter, entry);
	}

	return entry ? git_iterator_current(iter, entry) : 0;
}

int git_iterator_cmp(
	git_iterator *iter, const char *path_prefix)
{
	const git_index_entry *entry;

	/* a "done" iterator is after every prefix */
	if (git_iterator_current(iter, &entry) < 0 ||
		entry == NULL)
		return 1;

	/* a NULL prefix is after any valid iterator */
	if (!path_prefix)
		return -1;

	return git__prefixcmp(entry->path, path_prefix);
}

