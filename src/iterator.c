/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "iterator.h"
#include "tree.h"
#include "ignore.h"
#include "buffer.h"
#include "git2/submodule.h"
#include <ctype.h>

#define ITERATOR_SET_CB(P,NAME_LC) do { \
	(P)->cb.current = NAME_LC ## _iterator__current; \
	(P)->cb.at_end  = NAME_LC ## _iterator__at_end; \
	(P)->cb.advance = NAME_LC ## _iterator__advance; \
	(P)->cb.seek    = NAME_LC ## _iterator__seek; \
	(P)->cb.reset   = NAME_LC ## _iterator__reset; \
	(P)->cb.free    = NAME_LC ## _iterator__free; \
	} while (0)

#define ITERATOR_BASE_INIT(P,NAME_LC,NAME_UC) do { \
	(P) = git__calloc(1, sizeof(NAME_LC ## _iterator)); \
	GITERR_CHECK_ALLOC(P); \
	(P)->base.type    = GIT_ITERATOR_TYPE_ ## NAME_UC; \
	(P)->base.cb    = &(P)->cb; \
	ITERATOR_SET_CB(P,NAME_LC); \
	(P)->base.start   = start ? git__strdup(start) : NULL; \
	(P)->base.end     = end ? git__strdup(end) : NULL; \
	if ((start && !(P)->base.start) || (end && !(P)->base.end)) { \
		git__free(P); return -1; } \
	(P)->base.prefixcomp = git__prefixcmp; \
	} while (0)

static int iterator__reset_range(
	git_iterator *iter, const char *start, const char *end)
{
	if (start) {
		if (iter->start)
			git__free(iter->start);
		iter->start = git__strdup(start);
		GITERR_CHECK_ALLOC(iter->start);
	}

	if (end) {
		if (iter->end)
			git__free(iter->end);
		iter->end = git__strdup(end);
		GITERR_CHECK_ALLOC(iter->end);
	}

	return 0;
}

static int iterator_update_ignore_case(
	git_iterator *iter,
	git_iterator_flag_t flags)
{
	int error = 0, ignore_case = -1;

	if ((flags & GIT_ITERATOR_IGNORE_CASE) != 0)
		ignore_case = true;
	else if ((flags & GIT_ITERATOR_DONT_IGNORE_CASE) != 0)
		ignore_case = false;
	else {
		git_index *index;

		if (!(error = git_repository_index__weakptr(&index, iter->repo)))
			ignore_case = (index->ignore_case != false);
	}

	if (ignore_case > 0)
		iter->flags = (iter->flags | GIT_ITERATOR_IGNORE_CASE);
	else if (ignore_case == 0)
		iter->flags = (iter->flags & ~GIT_ITERATOR_IGNORE_CASE);

	iter->prefixcomp = ((iter->flags & GIT_ITERATOR_IGNORE_CASE) != 0) ?
		git__prefixcmp_icase : git__prefixcmp;

	return error;
}

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

static int empty_iterator__reset(
	git_iterator *iter, const char *start, const char *end)
{
	GIT_UNUSED(iter); GIT_UNUSED(start); GIT_UNUSED(end);
	return 0;
}

static int empty_iterator__seek(git_iterator *iter, const char *prefix)
{
	GIT_UNUSED(iter); GIT_UNUSED(prefix);
	return -1;
}

static void empty_iterator__free(git_iterator *iter)
{
	GIT_UNUSED(iter);
}

typedef struct {
	git_iterator base;
	git_iterator_callbacks cb;
} empty_iterator;

int git_iterator_for_nothing(git_iterator **iter, git_iterator_flag_t flags)
{
	empty_iterator *i = git__calloc(1, sizeof(empty_iterator));
	GITERR_CHECK_ALLOC(i);

	i->base.type  = GIT_ITERATOR_TYPE_EMPTY;
	i->base.cb    = &i->cb;
	i->base.flags = flags;
	i->cb.current = empty_iterator__no_item;
	i->cb.at_end  = empty_iterator__at_end;
	i->cb.advance = empty_iterator__no_item;
	i->cb.seek    = empty_iterator__seek;
	i->cb.reset   = empty_iterator__reset;
	i->cb.free    = empty_iterator__free;

	*iter = (git_iterator *)i;

	return 0;
}


typedef struct tree_iterator_frame tree_iterator_frame;
struct tree_iterator_frame {
	tree_iterator_frame *next, *prev;
	git_tree *tree;
	char *start;
	size_t startlen;
	size_t index;
	void **icase_map;
	void *icase_data[GIT_FLEX_ARRAY];
};

typedef struct {
	git_iterator base;
	git_iterator_callbacks cb;
	tree_iterator_frame *stack, *tail;
	git_index_entry entry;
	git_buf path;
	bool path_has_filename;
} tree_iterator;

GIT_INLINE(const git_tree_entry *)tree_iterator__tree_entry(tree_iterator *ti)
{
	tree_iterator_frame *tf = ti->stack;

	if (tf->index >= git_tree_entrycount(tf->tree))
		return NULL;

	return git_tree_entry_byindex(
		tf->tree, tf->icase_map ? (size_t)tf->icase_map[tf->index] : tf->index);
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

static void tree_iterator__free_frame(tree_iterator_frame *tf)
{
	if (!tf)
		return;

	git_tree_free(tf->tree);
	tf->tree = NULL;

	git__free(tf);
}

static bool tree_iterator__pop_frame(tree_iterator *ti)
{
	tree_iterator_frame *tf = ti->stack;

	/* don't free the initial tree/frame */
	if (!tf->next)
		return false;

	ti->stack = tf->next;
	ti->stack->prev = NULL;

	tree_iterator__free_frame(tf);

	return true;
}

static int tree_iterator__to_end(tree_iterator *ti)
{
	while (tree_iterator__pop_frame(ti)) /* pop all */;
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

	if (ti->base.end && ti->base.prefixcomp(ti->entry.path, ti->base.end) > 0)
		return tree_iterator__to_end(ti);

	if (entry)
		*entry = &ti->entry;

	return 0;
}

static int tree_iterator__at_end(git_iterator *self)
{
	return (tree_iterator__tree_entry((tree_iterator *)self) == NULL);
}

static int tree_iterator__icase_map_cmp(const void *a, const void *b, void *data)
{
	git_tree *tree = data;
	const git_tree_entry *te1 = git_tree_entry_byindex(tree, (size_t)a);
	const git_tree_entry *te2 = git_tree_entry_byindex(tree, (size_t)b);

	return te1 ? (te2 ? git_tree_entry_icmp(te1, te2) : 1) : -1;
}

static int tree_iterator__frame_start_icmp(const void *key, const void *el)
{
	const tree_iterator_frame *tf = (const tree_iterator_frame *)key;
	const git_tree_entry *te = git_tree_entry_byindex(tf->tree, (size_t)el);
	size_t minlen = min(tf->startlen, te->filename_len);

	return git__strncasecmp(tf->start, te->filename, minlen);
}

static void tree_iterator__frame_seek_start(tree_iterator_frame *tf)
{
	if (!tf->start)
		tf->index = 0;
	else if (!tf->icase_map)
		tf->index = git_tree__prefix_position(tf->tree, tf->start);
	else {
		if (!git__bsearch(
				tf->icase_map, git_tree_entrycount(tf->tree),
				tf, tree_iterator__frame_start_icmp, &tf->index))
		{
			while (tf->index > 0) {
				/* move back while previous entry is still prefixed */
				if (tree_iterator__frame_start_icmp(
						tf, (const void *)(tf->index - 1)))
					break;
				tf->index--;
			}
		}
	}
}

static tree_iterator_frame *tree_iterator__alloc_frame(
	tree_iterator *ti, git_tree *tree, char *start)
{
	size_t i, max_i = git_tree_entrycount(tree);
	tree_iterator_frame *tf =
		git__calloc(1, sizeof(tree_iterator_frame) + max_i * sizeof(void *));
	if (!tf)
		return NULL;

	tf->tree = tree;

	if (start && *start) {
		tf->start = start;
		tf->startlen = strlen(start);
	}

	if (!max_i)
		return tf;

	if ((ti->base.flags & GIT_ITERATOR_IGNORE_CASE) != 0) {
		tf->icase_map = tf->icase_data;

		for (i = 0; i < max_i; ++i)
			tf->icase_map[i] = (void *)i;

		git__tsort_r(
			tf->icase_map, max_i, tree_iterator__icase_map_cmp, tf->tree);
	}

	tree_iterator__frame_seek_start(tf);

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
			ti->base.prefixcomp(ti->path.ptr, ti->base.end) > 0)
			return tree_iterator__to_end(ti);

		if ((error = git_tree_lookup(&subtree, ti->base.repo, &te->oid)) < 0)
			return error;

		relpath = NULL;

		/* apply range start to new frame if relevant */
		if (ti->stack->start &&
			ti->base.prefixcomp(ti->stack->start, te->filename) == 0)
		{
			if (ti->stack->start[te->filename_len] == '/')
				relpath = ti->stack->start + te->filename_len + 1;
		}

		if ((tf = tree_iterator__alloc_frame(ti, subtree, relpath)) == NULL)
			return -1;

		tf->next  = ti->stack;
		ti->stack = tf;
		tf->next->prev = tf;

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

	while (1) {
		++ti->stack->index;

		if ((te = tree_iterator__tree_entry(ti)) != NULL)
			break;

		if (!tree_iterator__pop_frame(ti))
			break; /* no frames left to pop */

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

	while (tree_iterator__pop_frame(ti)) /* pop all */;

	tree_iterator__free_frame(ti->stack);
	ti->stack = ti->tail = NULL;

	git_buf_free(&ti->path);
}

static int tree_iterator__reset(
	git_iterator *self, const char *start, const char *end)
{
	tree_iterator *ti = (tree_iterator *)self;

	while (tree_iterator__pop_frame(ti)) /* pop all */;

	if (iterator__reset_range(self, start, end) < 0)
		return -1;

	/* reset start position */
	tree_iterator__frame_seek_start(ti->stack);

	git_buf_clear(&ti->path);
	ti->path_has_filename = false;

	return tree_iterator__expand_tree(ti);
}

int git_iterator_for_tree_range(
	git_iterator **iter,
	git_tree *tree,
	git_iterator_flag_t flags,
	const char *start,
	const char *end)
{
	int error;
	tree_iterator *ti;

	if (tree == NULL)
		return git_iterator_for_nothing(iter, flags);

	if ((error = git_tree__dup(&tree, tree)) < 0)
		return error;

	ITERATOR_BASE_INIT(ti, tree, TREE);

	ti->base.repo = git_tree_owner(tree);

	if ((error = iterator_update_ignore_case((git_iterator *)ti, flags)) < 0)
		goto fail;

	ti->stack = ti->tail = tree_iterator__alloc_frame(ti, tree, ti->base.start);

	if ((error = tree_iterator__expand_tree(ti)) < 0)
		goto fail;

	*iter = (git_iterator *)ti;
	return 0;

fail:
	git_iterator_free((git_iterator *)ti);
	return error;
}


typedef struct {
	git_iterator base;
	git_iterator_callbacks cb;
	git_index *index;
	size_t current;
} index_iterator;

static int index_iterator__current(
	git_iterator *self, const git_index_entry **entry)
{
	index_iterator *ii = (index_iterator *)self;
	const git_index_entry *ie = git_index_get_byindex(ii->index, ii->current);

	if (entry)
		*entry = ie;

	return 0;
}

static int index_iterator__at_end(git_iterator *self)
{
	index_iterator *ii = (index_iterator *)self;
	return (ii->current >= git_index_entrycount(ii->index));
}

static void index_iterator__skip_conflicts(
	index_iterator *ii)
{
	size_t entrycount = git_index_entrycount(ii->index);
	const git_index_entry *ie;

	while (ii->current < entrycount) {
		ie = git_index_get_byindex(ii->index, ii->current);

		if (ie == NULL ||
			(ii->base.end != NULL &&
			 ii->base.prefixcomp(ie->path, ii->base.end) > 0)) {
			ii->current = entrycount;
			break;
		}

		if (git_index_entry_stage(ie) == 0)
			break;

		ii->current++;
	}
}

static int index_iterator__advance(
	git_iterator *self, const git_index_entry **entry)
{
	index_iterator *ii = (index_iterator *)self;

	if (ii->current < git_index_entrycount(ii->index))
		ii->current++;

	index_iterator__skip_conflicts(ii);

	return index_iterator__current(self, entry);
}

static int index_iterator__seek(git_iterator *self, const char *prefix)
{
	GIT_UNUSED(self);
	GIT_UNUSED(prefix);
	/* find last item before prefix */
	return -1;
}

static int index_iterator__reset(
	git_iterator *self, const char *start, const char *end)
{
	index_iterator *ii = (index_iterator *)self;
	if (iterator__reset_range(self, start, end) < 0)
		return -1;
	ii->current = ii->base.start ?
		git_index__prefix_position(ii->index, ii->base.start) : 0;
	index_iterator__skip_conflicts(ii);
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
	git_index  *index,
	git_iterator_flag_t flags,
	const char *start,
	const char *end)
{
	index_iterator *ii;

	GIT_UNUSED(flags);

	ITERATOR_BASE_INIT(ii, index, INDEX);

	ii->base.repo = git_index_owner(index);
	if (index->ignore_case) {
		ii->base.flags |= GIT_ITERATOR_IGNORE_CASE;
		ii->base.prefixcomp = git__prefixcmp_icase;
	}
	ii->index = index;
	GIT_REFCOUNT_INC(index);

	index_iterator__reset((git_iterator *)ii, NULL, NULL);

	*iter = (git_iterator *)ii;

	return 0;
}


typedef struct workdir_iterator_frame workdir_iterator_frame;
struct workdir_iterator_frame {
	workdir_iterator_frame *next;
	git_vector entries;
	size_t index;
};

typedef struct {
	git_iterator base;
	git_iterator_callbacks cb;
	workdir_iterator_frame *stack;
	int (*entrycmp)(const void *pfx, const void *item);
	git_ignores ignores;
	git_index_entry entry;
	git_buf path;
	size_t root_len;
	int is_ignored;
} workdir_iterator;

GIT_INLINE(bool) path_is_dotgit(const git_path_with_stat *ps)
{
	if (!ps)
		return false;
	else {
		const char *path = ps->path;
		size_t len  = ps->path_len;

		if (len < 4)
			return false;
		if (path[len - 1] == '/')
			len--;
		if (tolower(path[len - 1]) != 't' ||
			tolower(path[len - 2]) != 'i' ||
			tolower(path[len - 3]) != 'g' ||
			tolower(path[len - 4]) != '.')
			return false;
		return (len == 4 || path[len - 5] == '/');
	}
}

static workdir_iterator_frame *workdir_iterator__alloc_frame(
	workdir_iterator *wi)
{
	workdir_iterator_frame *wf = git__calloc(1, sizeof(workdir_iterator_frame));
	git_vector_cmp entry_compare = CASESELECT(
		(wi->base.flags & GIT_ITERATOR_IGNORE_CASE) != 0,
		git_path_with_stat_cmp_icase, git_path_with_stat_cmp);

	if (wf == NULL)
		return NULL;

	if (git_vector_init(&wf->entries, 0, entry_compare) != 0) {
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

static int workdir_iterator__entry_cmp_case(const void *pfx, const void *item)
{
	const git_path_with_stat *ps = item;
	return git__prefixcmp((const char *)pfx, ps->path);
}

static int workdir_iterator__entry_cmp_icase(const void *pfx, const void *item)
{
	const git_path_with_stat *ps = item;
	return git__prefixcmp_icase((const char *)pfx, ps->path);
}

static void workdir_iterator__seek_frame_start(
	workdir_iterator *wi, workdir_iterator_frame *wf)
{
	if (!wf)
		return;

	if (wi->base.start)
		git_vector_bsearch2(
			&wf->index, &wf->entries, wi->entrycmp, wi->base.start);
	else
		wf->index = 0;

	if (path_is_dotgit(git_vector_get(&wf->entries, wf->index)))
		wf->index++;
}

static int workdir_iterator__expand_dir(workdir_iterator *wi)
{
	int error;
	workdir_iterator_frame *wf = workdir_iterator__alloc_frame(wi);
	GITERR_CHECK_ALLOC(wf);

	error = git_path_dirload_with_stat(
		wi->path.ptr, wi->root_len,
		(wi->base.flags & GIT_ITERATOR_IGNORE_CASE) != 0,
		wi->base.start, wi->base.end, &wf->entries);

	if (error < 0 || wf->entries.length == 0) {
		workdir_iterator__free_frame(wf);
		return GIT_ENOTFOUND;
	}

	workdir_iterator__seek_frame_start(wi, wf);

	/* only push new ignores if this is not top level directory */
	if (wi->stack != NULL) {
		ssize_t slash_pos = git_buf_rfind_next(&wi->path, '/');
		(void)git_ignore__push_dir(&wi->ignores, &wi->path.ptr[slash_pos + 1]);
	}

	wf->next  = wi->stack;
	wi->stack = wf;

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

	while (1) {
		wf   = wi->stack;
		next = git_vector_get(&wf->entries, ++wf->index);

		if (next != NULL) {
			/* match git's behavior of ignoring anything named ".git" */
			if (path_is_dotgit(next))
				continue;
			/* else found a good entry */
			break;
		}

		/* pop stack if anything is left to pop */
		if (!wf->next) {
			memset(&wi->entry, 0, sizeof(wi->entry));
			return 0;
		}

		wi->stack = wf->next;
		workdir_iterator__free_frame(wf);
		git_ignore__pop_dir(&wi->ignores);
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

static int workdir_iterator__reset(
	git_iterator *self, const char *start, const char *end)
{
	workdir_iterator *wi = (workdir_iterator *)self;

	while (wi->stack != NULL && wi->stack->next != NULL) {
		workdir_iterator_frame *wf = wi->stack;
		wi->stack = wf->next;
		workdir_iterator__free_frame(wf);
		git_ignore__pop_dir(&wi->ignores);
	}

	if (iterator__reset_range(self, start, end) < 0)
		return -1;

	workdir_iterator__seek_frame_start(wi, wi->stack);

	return workdir_iterator__update_entry(wi);
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
	git_path_with_stat *ps =
		git_vector_get(&wi->stack->entries, wi->stack->index);

	git_buf_truncate(&wi->path, wi->root_len);
	memset(&wi->entry, 0, sizeof(wi->entry));

	if (!ps)
		return 0;

	if (git_buf_put(&wi->path, ps->path, ps->path_len) < 0)
		return -1;

	if (wi->base.end &&
		wi->base.prefixcomp(wi->path.ptr + wi->root_len, wi->base.end) > 0)
		return 0;

	wi->entry.path = ps->path;

	/* skip over .git entries */
	if (path_is_dotgit(ps))
		return workdir_iterator__advance((git_iterator *)wi, NULL);

	wi->is_ignored = -1;

	git_index_entry__init_from_stat(&wi->entry, &ps->st);

	/* need different mode here to keep directories during iteration */
	wi->entry.mode = git_futils_canonical_mode(ps->st.st_mode);

	/* if this is a file type we don't handle, treat as ignored */
	if (wi->entry.mode == 0) {
		wi->is_ignored = 1;
		return 0;
	}

	/* detect submodules */
	if (S_ISDIR(wi->entry.mode)) {
		int res = git_submodule_lookup(NULL, wi->base.repo, wi->entry.path);
		bool is_submodule = (res == 0);
		if (res == GIT_ENOTFOUND)
			giterr_clear();

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
	git_iterator_flag_t flags,
	const char *start,
	const char *end)
{
	int error;
	workdir_iterator *wi;

	assert(iter && repo);

	if ((error = git_repository__ensure_not_bare(
			repo, "scan working directory")) < 0)
		return error;

	ITERATOR_BASE_INIT(wi, workdir, WORKDIR);
	wi->base.repo = repo;

	if ((error = iterator_update_ignore_case((git_iterator *)wi, flags)) < 0)
		goto fail;

	if (git_buf_sets(&wi->path, git_repository_workdir(repo)) < 0 ||
		git_path_to_dir(&wi->path) < 0 ||
		git_ignore__for_path(repo, "", &wi->ignores) < 0)
	{
		git__free(wi);
		return -1;
	}

	wi->root_len = wi->path.size;
	wi->entrycmp = (wi->base.flags & GIT_ITERATOR_IGNORE_CASE) != 0 ?
		workdir_iterator__entry_cmp_icase : workdir_iterator__entry_cmp_case;

	if ((error = workdir_iterator__expand_dir(wi)) < 0) {
		if (error != GIT_ENOTFOUND)
			goto fail;
		giterr_clear();
	}

	*iter = (git_iterator *)wi;
	return 0;

fail:
	git_iterator_free((git_iterator *)wi);
	return error;
}


typedef struct {
	/* replacement callbacks */
	git_iterator_callbacks cb;
	/* original iterator values */
	git_iterator_callbacks *orig;
	git_iterator_type_t orig_type;
	/* spoolandsort data */
	git_vector entries;
	git_pool entry_pool;
	git_pool string_pool;
	size_t position;
} spoolandsort_callbacks;

static int spoolandsort_iterator__current(
	git_iterator *self, const git_index_entry **entry)
{
	spoolandsort_callbacks *scb = (spoolandsort_callbacks *)self->cb;

	*entry = (const git_index_entry *)
		git_vector_get(&scb->entries, scb->position);

	return 0;
}

static int spoolandsort_iterator__at_end(git_iterator *self)
{
	spoolandsort_callbacks *scb = (spoolandsort_callbacks *)self->cb;

	return 0 == scb->entries.length || scb->entries.length - 1 <= scb->position;
}

static int spoolandsort_iterator__advance(
	git_iterator *self, const git_index_entry **entry)
{
	spoolandsort_callbacks *scb = (spoolandsort_callbacks *)self->cb;

	*entry = (const git_index_entry *)
		git_vector_get(&scb->entries, ++scb->position);

	return 0;
}

static int spoolandsort_iterator__seek(git_iterator *self, const char *prefix)
{
	GIT_UNUSED(self);
	GIT_UNUSED(prefix);

	return -1;
}

static int spoolandsort_iterator__reset(
	git_iterator *self, const char *start, const char *end)
{
	spoolandsort_callbacks *scb = (spoolandsort_callbacks *)self->cb;

	GIT_UNUSED(start); GIT_UNUSED(end);

	scb->position = 0;

	return 0;
}

static void spoolandsort_iterator__free_callbacks(spoolandsort_callbacks *scb)
{
	git_pool_clear(&scb->string_pool);
	git_pool_clear(&scb->entry_pool);
	git_vector_free(&scb->entries);
	git__free(scb);
}

void git_iterator_spoolandsort_pop(git_iterator *self)
{
	spoolandsort_callbacks *scb = (spoolandsort_callbacks *)self->cb;

	if (self->type != GIT_ITERATOR_TYPE_SPOOLANDSORT)
		return;

	self->cb   = scb->orig;
	self->type = scb->orig_type;
	self->flags ^= GIT_ITERATOR_IGNORE_CASE;

	spoolandsort_iterator__free_callbacks(scb);
}

static void spoolandsort_iterator__free(git_iterator *self)
{
	git_iterator_spoolandsort_pop(self);
	self->cb->free(self);
}

int git_iterator_spoolandsort_push(git_iterator *iter, bool ignore_case)
{
	const git_index_entry *item;
	spoolandsort_callbacks *scb;
	int (*entrycomp)(const void *a, const void *b);

	if (((iter->flags & GIT_ITERATOR_IGNORE_CASE) != 0) == (ignore_case != 0))
		return 0;

	if (iter->type == GIT_ITERATOR_TYPE_EMPTY) {
		iter->flags = (iter->flags ^ GIT_ITERATOR_IGNORE_CASE);
		return 0;
	}

	scb = git__calloc(1, sizeof(spoolandsort_callbacks));
	GITERR_CHECK_ALLOC(scb);

	ITERATOR_SET_CB(scb,spoolandsort);

	scb->orig      = iter->cb;
	scb->orig_type = iter->type;
	scb->position  = 0;

	entrycomp = ignore_case ? git_index_entry__cmp_icase : git_index_entry__cmp;

	if (git_vector_init(&scb->entries, 16, entrycomp) < 0 ||
		git_pool_init(&scb->entry_pool, sizeof(git_index_entry), 0) < 0 ||
		git_pool_init(&scb->string_pool, 1, 0) < 0 ||
		git_iterator_current(iter, &item) < 0)
		goto fail;

	while (item) {
		git_index_entry *clone = git_pool_malloc(&scb->entry_pool, 1);
		if (!clone)
			goto fail;

		memcpy(clone, item, sizeof(git_index_entry));

		if (item->path) {
			clone->path = git_pool_strdup(&scb->string_pool, item->path);
			if (!clone->path)
				goto fail;
		}

		if (git_vector_insert(&scb->entries, clone) < 0)
			goto fail;

		if (git_iterator_advance(iter, &item) < 0)
			goto fail;
	}

	git_vector_sort(&scb->entries);

	iter->cb   = (git_iterator_callbacks *)scb;
	iter->type = GIT_ITERATOR_TYPE_SPOOLANDSORT;
	iter->flags ^= GIT_ITERATOR_IGNORE_CASE;

	return 0;

fail:
	spoolandsort_iterator__free_callbacks(scb);
	return -1;
}


void git_iterator_free(git_iterator *iter)
{
	if (iter == NULL)
		return;

	iter->cb->free(iter);

	git__free(iter->start);
	git__free(iter->end);

	memset(iter, 0, sizeof(*iter));

	git__free(iter);
}

git_index *git_iterator_index_get_index(git_iterator *iter)
{
	if (iter->type == GIT_ITERATOR_TYPE_INDEX)
		return ((index_iterator *)iter)->index;

	if (iter->type == GIT_ITERATOR_TYPE_SPOOLANDSORT &&
		((spoolandsort_callbacks *)iter->cb)->orig_type == GIT_ITERATOR_TYPE_INDEX)
		return ((index_iterator *)iter)->index;

	return NULL;
}

git_iterator_type_t git_iterator_inner_type(git_iterator *iter)
{
	if (iter->type == GIT_ITERATOR_TYPE_SPOOLANDSORT)
		return ((spoolandsort_callbacks *)iter->cb)->orig_type;

	return iter->type;
}

int git_iterator_current_tree_entry(
	git_iterator *iter, const git_tree_entry **tree_entry)
{
	*tree_entry = (iter->type != GIT_ITERATOR_TYPE_TREE) ? NULL :
		tree_iterator__tree_entry((tree_iterator *)iter);
	return 0;
}

int git_iterator_current_parent_tree(
	git_iterator *iter,
	const char *parent_path,
	const git_tree **tree_ptr)
{
	tree_iterator *ti = (tree_iterator *)iter;
	tree_iterator_frame *tf;
	const char *scan = parent_path;
	int (*strncomp)(const char *a, const char *b, size_t sz);

	if (iter->type != GIT_ITERATOR_TYPE_TREE || ti->stack == NULL)
		goto notfound;

	strncomp = ((iter->flags & GIT_ITERATOR_IGNORE_CASE) != 0) ?
		git__strncasecmp : git__strncmp;

	for (tf = ti->tail; tf != NULL; tf = tf->prev) {
		const git_tree_entry *te;

		if (!*scan) {
			*tree_ptr = tf->tree;
			return 0;
		}

		te = git_tree_entry_byindex(tf->tree,
			tf->icase_map ? (size_t)tf->icase_map[tf->index] : tf->index);

		if (strncomp(scan, te->filename, te->filename_len) != 0)
			goto notfound;

		scan += te->filename_len;

		if (*scan) {
			if (*scan != '/')
				goto notfound;
			scan++;
		}
	}

notfound:
	*tree_ptr = NULL;
	return 0;
}

int git_iterator_current_is_ignored(git_iterator *iter)
{
	workdir_iterator *wi = (workdir_iterator *)iter;

	if (iter->type != GIT_ITERATOR_TYPE_WORKDIR)
		return 0;

	if (wi->is_ignored != -1)
		return wi->is_ignored;

	if (git_ignore__lookup(&wi->ignores, wi->entry.path, &wi->is_ignored) < 0)
		wi->is_ignored = 1;

	return wi->is_ignored;
}

int git_iterator_advance_into_directory(
	git_iterator *iter, const git_index_entry **entry)
{
	workdir_iterator *wi = (workdir_iterator *)iter;

	if (iter->type == GIT_ITERATOR_TYPE_WORKDIR &&
		wi->entry.path &&
		(wi->entry.mode == GIT_FILEMODE_TREE ||
		 wi->entry.mode == GIT_FILEMODE_COMMIT))
	{
		if (workdir_iterator__expand_dir(wi) < 0)
			/* if error loading or if empty, skip the directory. */
			return workdir_iterator__advance(iter, entry);
	}

	return entry ? git_iterator_current(iter, entry) : 0;
}

int git_iterator_cmp(git_iterator *iter, const char *path_prefix)
{
	const git_index_entry *entry;

	/* a "done" iterator is after every prefix */
	if (git_iterator_current(iter, &entry) < 0 ||
		entry == NULL)
		return 1;

	/* a NULL prefix is after any valid iterator */
	if (!path_prefix)
		return -1;

	return iter->prefixcomp(entry->path, path_prefix);
}

int git_iterator_current_workdir_path(git_iterator *iter, git_buf **path)
{
	workdir_iterator *wi = (workdir_iterator *)iter;

	if (iter->type != GIT_ITERATOR_TYPE_WORKDIR || !wi->entry.path)
		*path = NULL;
	else
		*path = &wi->path;

	return 0;
}

