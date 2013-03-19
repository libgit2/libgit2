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
	(P)->cb.advance = NAME_LC ## _iterator__advance; \
	(P)->cb.advance_into = NAME_LC ## _iterator__advance_into; \
	(P)->cb.seek    = NAME_LC ## _iterator__seek; \
	(P)->cb.reset   = NAME_LC ## _iterator__reset; \
	(P)->cb.at_end  = NAME_LC ## _iterator__at_end; \
	(P)->cb.free    = NAME_LC ## _iterator__free; \
	} while (0)

#define ITERATOR_CASE_FLAGS \
	(GIT_ITERATOR_IGNORE_CASE | GIT_ITERATOR_DONT_IGNORE_CASE)

#define ITERATOR_BASE_INIT(P,NAME_LC,NAME_UC,REPO) do { \
	(P) = git__calloc(1, sizeof(NAME_LC ## _iterator)); \
	GITERR_CHECK_ALLOC(P); \
	(P)->base.type    = GIT_ITERATOR_TYPE_ ## NAME_UC; \
	(P)->base.cb      = &(P)->cb; \
	ITERATOR_SET_CB(P,NAME_LC); \
	(P)->base.repo    = (REPO); \
	(P)->base.start   = start ? git__strdup(start) : NULL; \
	(P)->base.end     = end ? git__strdup(end) : NULL; \
	if ((start && !(P)->base.start) || (end && !(P)->base.end)) { \
		git__free(P); return -1; } \
	(P)->base.prefixcomp = git__prefixcmp; \
	(P)->base.flags = flags & ~ITERATOR_CASE_FLAGS; \
	if ((P)->base.flags & GIT_ITERATOR_DONT_AUTOEXPAND) \
		(P)->base.flags |= GIT_ITERATOR_INCLUDE_TREES; \
	} while (0)

#define iterator__flag(I,F) ((((git_iterator *)(I))->flags & GIT_ITERATOR_ ## F) != 0)
#define iterator__ignore_case(I)     iterator__flag(I,IGNORE_CASE)
#define iterator__include_trees(I)   iterator__flag(I,INCLUDE_TREES)
#define iterator__dont_autoexpand(I) iterator__flag(I,DONT_AUTOEXPAND)
#define iterator__do_autoexpand(I)   !iterator__flag(I,DONT_AUTOEXPAND)

#define iterator__end(I) ((git_iterator *)(I))->end
#define iterator__past_end(I,PATH) \
	(iterator__end(I) && ((git_iterator *)(I))->prefixcomp((PATH),iterator__end(I)) > 0)


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

static int iterator__update_ignore_case(
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

	iter->prefixcomp = iterator__ignore_case(iter) ?
		git__prefixcmp_icase : git__prefixcmp;

	return error;
}

GIT_INLINE(void) iterator__clear_entry(const git_index_entry **entry)
{
	if (entry) *entry = NULL;
}


static int empty_iterator__noop(const git_index_entry **e, git_iterator *i)
{
	GIT_UNUSED(i);
	iterator__clear_entry(e);
	return 0;
}

static int empty_iterator__seek(git_iterator *i, const char *p)
{
	GIT_UNUSED(i); GIT_UNUSED(p);
	return -1;
}

static int empty_iterator__reset(git_iterator *i, const char *s, const char *e)
{
	GIT_UNUSED(i); GIT_UNUSED(s); GIT_UNUSED(e);
	return 0;
}

static int empty_iterator__at_end(git_iterator *i)
{
	GIT_UNUSED(i);
	return 1;
}

static void empty_iterator__free(git_iterator *i)
{
	GIT_UNUSED(i);
}

typedef struct {
	git_iterator base;
	git_iterator_callbacks cb;
} empty_iterator;

int git_iterator_for_nothing(
	git_iterator **iter,
	git_iterator_flag_t flags,
	const char *start,
	const char *end)
{
	empty_iterator *i;

#define empty_iterator__current empty_iterator__noop
#define empty_iterator__advance empty_iterator__noop
#define empty_iterator__advance_into empty_iterator__noop

	ITERATOR_BASE_INIT(i, empty, EMPTY, NULL);

	if ((flags & GIT_ITERATOR_IGNORE_CASE) != 0)
		i->base.flags |= GIT_ITERATOR_IGNORE_CASE;

	*iter = (git_iterator *)i;
	return 0;
}


typedef struct tree_iterator_entry tree_iterator_entry;
struct tree_iterator_entry {
	tree_iterator_entry *parent;
	const git_tree_entry *te;
	git_tree *tree;
};

typedef struct tree_iterator_frame tree_iterator_frame;
struct tree_iterator_frame {
	tree_iterator_frame *up, *down;

	size_t n_entries; /* items in this frame */
	size_t current;   /* start of currently active range in frame */
	size_t next;      /* start of next range in frame */

	const char *start;
	size_t startlen;

	tree_iterator_entry *entries[GIT_FLEX_ARRAY];
};

typedef struct {
	git_iterator base;
	git_iterator_callbacks cb;
	tree_iterator_frame *head, *root;
	git_pool pool;
	git_index_entry entry;
	git_buf path;
	int path_ambiguities;
	bool path_has_filename;
	int (*strncomp)(const char *a, const char *b, size_t sz);
} tree_iterator;

static char *tree_iterator__current_filename(
	tree_iterator *ti, const git_tree_entry *te)
{
	if (!ti->path_has_filename) {
		if (git_buf_joinpath(&ti->path, ti->path.ptr, te->filename) < 0)
			return NULL;

		if (git_tree_entry__is_tree(te) && git_buf_putc(&ti->path, '/') < 0)
			return NULL;

		ti->path_has_filename = true;
	}

	return ti->path.ptr;
}

static void tree_iterator__rewrite_filename(tree_iterator *ti)
{
	tree_iterator_entry *scan = ti->head->entries[ti->head->current];
	ssize_t strpos = ti->path.size;
	const git_tree_entry *te;

	if (strpos && ti->path.ptr[strpos - 1] == '/')
		strpos--;

	for (; scan && (te = scan->te); scan = scan->parent) {
		strpos -= te->filename_len;
		memcpy(&ti->path.ptr[strpos], te->filename, te->filename_len);
		strpos -= 1; /* separator */
	}
}

static int tree_iterator__te_cmp(
	const git_tree_entry *a,
	const git_tree_entry *b,
	int (*compare)(const char *, const char *, size_t))
{
	return git_path_cmp(
		a->filename, a->filename_len, a->attr == GIT_FILEMODE_TREE,
		b->filename, b->filename_len, b->attr == GIT_FILEMODE_TREE,
		compare);
}

static int tree_iterator__ci_cmp(const void *a, const void *b, void *p)
{
	const tree_iterator_entry *ae = a, *be = b;
	int cmp = tree_iterator__te_cmp(ae->te, be->te, git__strncasecmp);

	if (!cmp) {
		/* stabilize sort order among equivalent names */
		if (!ae->parent->te || !be->parent->te)
			cmp = tree_iterator__te_cmp(ae->te, be->te, git__strncmp);
		else
			cmp = tree_iterator__ci_cmp(ae->parent, be->parent, p);
	}

	return cmp;
}

static int tree_iterator__search_cmp(const void *key, const void *val, void *p)
{
	const tree_iterator_frame *tf = key;
	const git_tree_entry *te = ((tree_iterator_entry *)val)->te;

	return git_path_cmp(
		tf->start, tf->startlen, false,
		te->filename, te->filename_len, te->attr == GIT_FILEMODE_TREE,
		((tree_iterator *)p)->strncomp);
}

static int tree_iterator__set_next(tree_iterator *ti, tree_iterator_frame *tf)
{
	int error;
	const git_tree_entry *te, *last = NULL;

	tf->next = tf->current;

	for (; tf->next < tf->n_entries; tf->next++, last = te) {
		te = tf->entries[tf->next]->te;

		if (last && tree_iterator__te_cmp(last, te, ti->strncomp))
			break;

		/* load trees for items in [current,next) range */
		if (git_tree_entry__is_tree(te) &&
			(error = git_tree_lookup(
				&tf->entries[tf->next]->tree, ti->base.repo, &te->oid)) < 0)
			return error;
	}

	if (tf->next > tf->current + 1)
		ti->path_ambiguities++;

	if (last && !tree_iterator__current_filename(ti, last))
		return -1;

	return 0;
}

GIT_INLINE(bool) tree_iterator__at_tree(tree_iterator *ti)
{
	return (ti->head->current < ti->head->n_entries &&
			ti->head->entries[ti->head->current]->tree != NULL);
}

static int tree_iterator__push_frame(tree_iterator *ti)
{
	int error = 0;
	tree_iterator_frame *head = ti->head, *tf = NULL;
	size_t i, n_entries = 0;

	if (head->current >= head->n_entries || !head->entries[head->current]->tree)
		return 0;

	for (i = head->current; i < head->next; ++i)
		n_entries += git_tree_entrycount(head->entries[i]->tree);

	tf = git__calloc(sizeof(tree_iterator_frame) +
		n_entries * sizeof(tree_iterator_entry *), 1);
	GITERR_CHECK_ALLOC(tf);

	tf->n_entries = n_entries;

	tf->up     = head;
	head->down = tf;
	ti->head   = tf;

	for (i = head->current, n_entries = 0; i < head->next; ++i) {
		git_tree *tree = head->entries[i]->tree;
		size_t j, max_j = git_tree_entrycount(tree);

		for (j = 0; j < max_j; ++j) {
			tree_iterator_entry *entry = git_pool_malloc(&ti->pool, 1);
			GITERR_CHECK_ALLOC(entry);

			entry->parent = head->entries[i];
			entry->te     = git_tree_entry_byindex(tree, j);
			entry->tree   = NULL;

			tf->entries[n_entries++] = entry;
		}
	}

	/* if ignore_case, sort entries case insensitively */
	if (iterator__ignore_case(ti))
		git__tsort_r(
			(void **)tf->entries, tf->n_entries, tree_iterator__ci_cmp, tf);

	/* pick tf->current based on "start" (or start at zero) */
	if (head->startlen > 0) {
		git__bsearch_r((void **)tf->entries, tf->n_entries, head,
			tree_iterator__search_cmp, ti, &tf->current);

		while (tf->current &&
			   !tree_iterator__search_cmp(head, tf->entries[tf->current-1], ti))
			tf->current--;

		if ((tf->start = strchr(head->start, '/')) != NULL) {
			tf->start++;
			tf->startlen = strlen(tf->start);
		}
	}

	ti->path_has_filename = false;

	if ((error = tree_iterator__set_next(ti, tf)) < 0)
		return error;

	/* autoexpand as needed */
	if (!iterator__include_trees(ti) && tree_iterator__at_tree(ti))
		return tree_iterator__push_frame(ti);

	return 0;
}

static bool tree_iterator__move_to_next(
	tree_iterator *ti, tree_iterator_frame *tf)
{
	if (tf->next > tf->current + 1)
		ti->path_ambiguities--;

	if (!tf->up) { /* at root */
		tf->current = tf->next;
		return false;
	}

	for (; tf->current < tf->next; tf->current++) {
		git_tree_free(tf->entries[tf->current]->tree);
		tf->entries[tf->current]->tree = NULL;
	}

	return (tf->current < tf->n_entries);
}

static bool tree_iterator__pop_frame(tree_iterator *ti, bool final)
{
	tree_iterator_frame *tf = ti->head;

	if (!tf->up)
		return false;

	ti->head = tf->up;
	ti->head->down = NULL;

	tree_iterator__move_to_next(ti, tf);

	if (!final) { /* if final, don't bother to clean up */
		git_pool_free_array(&ti->pool, tf->n_entries, (void **)tf->entries);
		git_buf_rtruncate_at_char(&ti->path, '/');
	}

	git__free(tf);

	return true;
}

static int tree_iterator__pop_all(tree_iterator *ti, bool to_end, bool final)
{
	while (tree_iterator__pop_frame(ti, final)) /* pop to root */;

	if (!final) {
		ti->head->current = to_end ? ti->head->n_entries : 0;
		ti->path_ambiguities = 0;
		git_buf_clear(&ti->path);
	}

	return 0;
}

static int tree_iterator__current(
	const git_index_entry **entry, git_iterator *self)
{
	tree_iterator *ti = (tree_iterator *)self;
	tree_iterator_frame *tf = ti->head;
	const git_tree_entry *te;

	iterator__clear_entry(entry);

	if (tf->current >= tf->n_entries)
		return 0;
	te = tf->entries[tf->current]->te;

	ti->entry.mode = te->attr;
	git_oid_cpy(&ti->entry.oid, &te->oid);

	ti->entry.path = tree_iterator__current_filename(ti, te);
	GITERR_CHECK_ALLOC(ti->entry.path);

	if (ti->path_ambiguities > 0)
		tree_iterator__rewrite_filename(ti);

	if (iterator__past_end(ti, ti->entry.path))
		return tree_iterator__pop_all(ti, true, false);

	if (entry)
		*entry = &ti->entry;

	return 0;
}

static int tree_iterator__advance_into(
	const git_index_entry **entry, git_iterator *self)
{
	int error = 0;
	tree_iterator *ti = (tree_iterator *)self;

	iterator__clear_entry(entry);

	if (tree_iterator__at_tree(ti) &&
		!(error = tree_iterator__push_frame(ti)))
		error = tree_iterator__current(entry, self);

	return error;
}

static int tree_iterator__advance(
	const git_index_entry **entry, git_iterator *self)
{
	int error;
	tree_iterator *ti = (tree_iterator *)self;
	tree_iterator_frame *tf = ti->head;

	iterator__clear_entry(entry);

	if (tf->current > tf->n_entries)
		return 0;

	if (iterator__do_autoexpand(ti) && iterator__include_trees(ti) &&
		tree_iterator__at_tree(ti))
		return tree_iterator__advance_into(entry, self);

	if (ti->path_has_filename) {
		git_buf_rtruncate_at_char(&ti->path, '/');
		ti->path_has_filename = false;
	}

	/* scan forward and up, advancing in frame or popping frame when done */
	while (!tree_iterator__move_to_next(ti, tf) &&
		   tree_iterator__pop_frame(ti, false))
		tf = ti->head;

	/* find next and load trees */
	if ((error = tree_iterator__set_next(ti, tf)) < 0)
		return error;

	/* deal with include_trees / auto_expand as needed */
	if (!iterator__include_trees(ti) && tree_iterator__at_tree(ti))
		return tree_iterator__advance_into(entry, self);

	return tree_iterator__current(entry, self);
}

static int tree_iterator__seek(git_iterator *self, const char *prefix)
{
	GIT_UNUSED(self); GIT_UNUSED(prefix);
	return -1;
}

static int tree_iterator__reset(
	git_iterator *self, const char *start, const char *end)
{
	tree_iterator *ti = (tree_iterator *)self;

	tree_iterator__pop_all(ti, false, false);

	if (iterator__reset_range(self, start, end) < 0)
		return -1;

	return tree_iterator__push_frame(ti); /* re-expand root tree */
}

static int tree_iterator__at_end(git_iterator *self)
{
	tree_iterator *ti = (tree_iterator *)self;
	return (ti->head->current >= ti->head->n_entries);
}

static void tree_iterator__free(git_iterator *self)
{
	tree_iterator *ti = (tree_iterator *)self;

	tree_iterator__pop_all(ti, true, false);

	git_tree_free(ti->head->entries[0]->tree);
	git__free(ti->head);
	git_pool_clear(&ti->pool);
	git_buf_free(&ti->path);
}

static int tree_iterator__create_root_frame(tree_iterator *ti, git_tree *tree)
{
	size_t sz = sizeof(tree_iterator_frame) + sizeof(tree_iterator_entry);
	tree_iterator_frame *root = git__calloc(sz, sizeof(char));
	GITERR_CHECK_ALLOC(root);

	root->n_entries  = 1;
	root->next       = 1;
	root->start      = ti->base.start;
	root->startlen   = root->start ? strlen(root->start) : 0;
	root->entries[0] = git_pool_mallocz(&ti->pool, 1);
	GITERR_CHECK_ALLOC(root->entries[0]);
	root->entries[0]->tree = tree;

	ti->head = ti->root = root;

	return 0;
}

int git_iterator_for_tree(
	git_iterator **iter,
	git_tree *tree,
	git_iterator_flag_t flags,
	const char *start,
	const char *end)
{
	int error;
	tree_iterator *ti;

	if (tree == NULL)
		return git_iterator_for_nothing(iter, flags, start, end);

	if ((error = git_tree__dup(&tree, tree)) < 0)
		return error;

	ITERATOR_BASE_INIT(ti, tree, TREE, git_tree_owner(tree));

	if ((error = iterator__update_ignore_case((git_iterator *)ti, flags)) < 0)
		goto fail;
	ti->strncomp = iterator__ignore_case(ti) ? git__strncasecmp : git__strncmp;

	if ((error = git_pool_init(&ti->pool, sizeof(tree_iterator_entry),0)) < 0 ||
		(error = tree_iterator__create_root_frame(ti, tree)) < 0 ||
		(error = tree_iterator__push_frame(ti)) < 0) /* expand root now */
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
	/* when not in autoexpand mode, use these to represent "tree" state */
	git_buf partial;
	size_t partial_pos;
	char restore_terminator;
	git_index_entry tree_entry;
} index_iterator;

static const git_index_entry *index_iterator__index_entry(index_iterator *ii)
{
	const git_index_entry *ie = git_index_get_byindex(ii->index, ii->current);

	if (ie != NULL && iterator__past_end(ii, ie->path)) {
		ii->current = git_index_entrycount(ii->index);
		ie = NULL;
	}

	return ie;
}

static const git_index_entry *index_iterator__skip_conflicts(index_iterator *ii)
{
	const git_index_entry *ie;

	while ((ie = index_iterator__index_entry(ii)) != NULL &&
		   git_index_entry_stage(ie) != 0)
		ii->current++;

	return ie;
}

static void index_iterator__next_prefix_tree(index_iterator *ii)
{
	const char *slash;

	if (!iterator__include_trees(ii))
		return;

	slash = strchr(&ii->partial.ptr[ii->partial_pos], '/');

	if (slash != NULL) {
		ii->partial_pos = (slash - ii->partial.ptr) + 1;
		ii->restore_terminator = ii->partial.ptr[ii->partial_pos];
		ii->partial.ptr[ii->partial_pos] = '\0';
	} else {
		ii->partial_pos = ii->partial.size;
	}

	if (index_iterator__index_entry(ii) == NULL)
		ii->partial_pos = ii->partial.size;
}

static int index_iterator__first_prefix_tree(index_iterator *ii)
{
	const git_index_entry *ie = index_iterator__skip_conflicts(ii);
	const char *scan, *prior, *slash;

	if (!ie || !iterator__include_trees(ii))
		return 0;

	/* find longest common prefix with prior index entry */
	for (scan = slash = ie->path, prior = ii->partial.ptr;
		 *scan && *scan == *prior; ++scan, ++prior)
		if (*scan == '/')
			slash = scan;

	if (git_buf_sets(&ii->partial, ie->path) < 0)
		return -1;

	ii->partial_pos = (slash - ie->path) + 1;
	index_iterator__next_prefix_tree(ii);

	return 0;
}

#define index_iterator__at_tree(I) \
	(iterator__include_trees(I) && (I)->partial_pos < (I)->partial.size)

static int index_iterator__current(
	const git_index_entry **entry, git_iterator *self)
{
	index_iterator *ii = (index_iterator *)self;
	const git_index_entry *ie = git_index_get_byindex(ii->index, ii->current);

	if (ie != NULL && index_iterator__at_tree(ii)) {
		ii->tree_entry.path = ii->partial.ptr;
		ie = &ii->tree_entry;
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
	const git_index_entry **entry, git_iterator *self)
{
	index_iterator *ii = (index_iterator *)self;
	size_t entrycount = git_index_entrycount(ii->index);
	const git_index_entry *ie;

	if (index_iterator__at_tree(ii)) {
		if (iterator__do_autoexpand(ii)) {
			ii->partial.ptr[ii->partial_pos] = ii->restore_terminator;
			index_iterator__next_prefix_tree(ii);
		} else {
			/* advance to sibling tree (i.e. find entry with new prefix) */
			while (ii->current < entrycount) {
				ii->current++;

				if (!(ie = git_index_get_byindex(ii->index, ii->current)) ||
					ii->base.prefixcomp(ie->path, ii->partial.ptr) != 0)
					break;
			}

			if (index_iterator__first_prefix_tree(ii) < 0)
				return -1;
		}
	} else {
		if (ii->current < entrycount)
			ii->current++;

		if (index_iterator__first_prefix_tree(ii) < 0)
			return -1;
	}

	return index_iterator__current(entry, self);
}

static int index_iterator__advance_into(
	const git_index_entry **entry, git_iterator *self)
{
	index_iterator *ii = (index_iterator *)self;
	const git_index_entry *ie = git_index_get_byindex(ii->index, ii->current);

	if (ie != NULL && index_iterator__at_tree(ii)) {
		if (ii->restore_terminator)
			ii->partial.ptr[ii->partial_pos] = ii->restore_terminator;
		index_iterator__next_prefix_tree(ii);
	}

	return index_iterator__current(entry, self);
}

static int index_iterator__seek(git_iterator *self, const char *prefix)
{
	GIT_UNUSED(self); GIT_UNUSED(prefix);
	return -1;
}

static int index_iterator__reset(
	git_iterator *self, const char *start, const char *end)
{
	index_iterator *ii = (index_iterator *)self;
	const git_index_entry *ie;

	if (iterator__reset_range(self, start, end) < 0)
		return -1;

	ii->current = ii->base.start ?
		git_index__prefix_position(ii->index, ii->base.start) : 0;

	if ((ie = index_iterator__skip_conflicts(ii)) == NULL)
		return 0;

	if (git_buf_sets(&ii->partial, ie->path) < 0)
		return -1;

	ii->partial_pos = 0;

	if (ii->base.start) {
		size_t startlen = strlen(ii->base.start);

		ii->partial_pos = (startlen > ii->partial.size) ?
			ii->partial.size : startlen;
	}

	index_iterator__next_prefix_tree(ii);

	return 0;
}

static void index_iterator__free(git_iterator *self)
{
	index_iterator *ii = (index_iterator *)self;
	git_index_free(ii->index);
	ii->index = NULL;

	git_buf_free(&ii->partial);
}

int git_iterator_for_index(
	git_iterator **iter,
	git_index  *index,
	git_iterator_flag_t flags,
	const char *start,
	const char *end)
{
	index_iterator *ii;

	ITERATOR_BASE_INIT(ii, index, INDEX, git_index_owner(index));

	if (index->ignore_case) {
		ii->base.flags |= GIT_ITERATOR_IGNORE_CASE;
		ii->base.prefixcomp = git__prefixcmp_icase;
	}

	ii->index = index;
	GIT_REFCOUNT_INC(index);

	git_buf_init(&ii->partial, 0);
	ii->tree_entry.mode = GIT_FILEMODE_TREE;

	index_iterator__reset((git_iterator *)ii, NULL, NULL);

	*iter = (git_iterator *)ii;

	return 0;
}


#define WORKDIR_MAX_DEPTH 100

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
	git_ignores ignores;
	git_index_entry entry;
	git_buf path;
	size_t root_len;
	int is_ignored;
	int depth;
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
		iterator__ignore_case(wi),
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

static int workdir_iterator__entry_cmp(const void *i, const void *item)
{
	const workdir_iterator *wi = (const workdir_iterator *)i;
	const git_path_with_stat *ps = item;
	return wi->base.prefixcomp(wi->base.start, ps->path);
}

static void workdir_iterator__seek_frame_start(
	workdir_iterator *wi, workdir_iterator_frame *wf)
{
	if (!wf)
		return;

	if (wi->base.start)
		git_vector_bsearch2(
			&wf->index, &wf->entries, workdir_iterator__entry_cmp, wi);
	else
		wf->index = 0;

	if (path_is_dotgit(git_vector_get(&wf->entries, wf->index)))
		wf->index++;
}

static int workdir_iterator__expand_dir(workdir_iterator *wi)
{
	int error;
	workdir_iterator_frame *wf;

	wf = workdir_iterator__alloc_frame(wi);
	GITERR_CHECK_ALLOC(wf);

	error = git_path_dirload_with_stat(
		wi->path.ptr, wi->root_len, iterator__ignore_case(wi),
		wi->base.start, wi->base.end, &wf->entries);

	if (error < 0 || wf->entries.length == 0) {
		workdir_iterator__free_frame(wf);
		return GIT_ENOTFOUND;
	}

	if (++(wi->depth) > WORKDIR_MAX_DEPTH) {
		giterr_set(GITERR_REPOSITORY,
			"Working directory is too deep (%d)", wi->depth);
		workdir_iterator__free_frame(wf);
		return -1;
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
	const git_index_entry **entry, git_iterator *self)
{
	workdir_iterator *wi = (workdir_iterator *)self;
	if (entry)
		*entry = (wi->entry.path == NULL) ? NULL : &wi->entry;
	return 0;
}

static int workdir_iterator__at_end(git_iterator *self)
{
	return (((workdir_iterator *)self)->entry.path == NULL);
}

static int workdir_iterator__advance_into(
	const git_index_entry **entry, git_iterator *iter)
{
	int error = 0;
	workdir_iterator *wi = (workdir_iterator *)iter;

	iterator__clear_entry(entry);

	/* workdir iterator will allow you to explicitly advance into a
	 * commit/submodule (as well as a tree) to avoid some cases where an
	 * entry is mislabeled as a submodule in the working directory
	 */
	if (wi->entry.path != NULL &&
		(wi->entry.mode == GIT_FILEMODE_TREE ||
		 wi->entry.mode == GIT_FILEMODE_COMMIT))
		/* returns GIT_ENOTFOUND if the directory is empty */
		error = workdir_iterator__expand_dir(wi);

	if (!error && entry)
		error = workdir_iterator__current(entry, iter);

	return error;
}

static int workdir_iterator__advance(
	const git_index_entry **entry, git_iterator *self)
{
	int error = 0;
	workdir_iterator *wi = (workdir_iterator *)self;
	workdir_iterator_frame *wf;
	git_path_with_stat *next;

	/* given include_trees & autoexpand, we might have to go into a tree */
	if (iterator__do_autoexpand(wi) &&
		wi->entry.path != NULL &&
		wi->entry.mode == GIT_FILEMODE_TREE)
	{
		error = workdir_iterator__advance_into(entry, self);

		/* continue silently past empty directories if autoexpanding */
		if (error != GIT_ENOTFOUND)
			return error;
		giterr_clear();
		error = 0;
	}

	if (entry != NULL)
		*entry = NULL;

	while (wi->entry.path != NULL) {
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
		wi->depth--;
		workdir_iterator__free_frame(wf);
		git_ignore__pop_dir(&wi->ignores);
	}

	error = workdir_iterator__update_entry(wi);

	if (!error && entry != NULL)
		error = workdir_iterator__current(entry, self);

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
	wi->depth = 0;

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
	int error = 0;
	git_path_with_stat *ps =
		git_vector_get(&wi->stack->entries, wi->stack->index);

	git_buf_truncate(&wi->path, wi->root_len);
	memset(&wi->entry, 0, sizeof(wi->entry));

	if (!ps)
		return 0;

	/* skip over .git entries */
	if (path_is_dotgit(ps))
		return workdir_iterator__advance(NULL, (git_iterator *)wi);

	if (git_buf_put(&wi->path, ps->path, ps->path_len) < 0)
		return -1;

	if (iterator__past_end(wi, wi->path.ptr + wi->root_len))
		return 0;

	wi->entry.path = ps->path;

	wi->is_ignored = -1;

	git_index_entry__init_from_stat(&wi->entry, &ps->st);

	/* need different mode here to keep directories during iteration */
	wi->entry.mode = git_futils_canonical_mode(ps->st.st_mode);

	/* if this is a file type we don't handle, treat as ignored */
	if (wi->entry.mode == 0) {
		wi->is_ignored = 1;
		return 0;
	}

	/* if this isn't a tree, then we're done */
	if (wi->entry.mode != GIT_FILEMODE_TREE)
		return 0;

	/* detect submodules */
	error = git_submodule_lookup(NULL, wi->base.repo, wi->entry.path);
	if (error == GIT_ENOTFOUND)
		giterr_clear();

	if (error == GIT_EEXISTS) /* if contains .git, treat as untracked submod */
		error = 0;

	/* if submodule, mark as GITLINK and remove trailing slash */
	if (!error) {
		size_t len = strlen(wi->entry.path);
		assert(wi->entry.path[len - 1] == '/');
		wi->entry.path[len - 1] = '\0';
		wi->entry.mode = S_IFGITLINK;
		return 0;
	}

	if (iterator__include_trees(wi))
		return 0;

	return workdir_iterator__advance(NULL, (git_iterator *)wi);
}

int git_iterator_for_workdir(
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

	ITERATOR_BASE_INIT(wi, workdir, WORKDIR, repo);

	if ((error = iterator__update_ignore_case((git_iterator *)wi, flags)) < 0)
		goto fail;

	if (git_buf_sets(&wi->path, git_repository_workdir(repo)) < 0 ||
		git_path_to_dir(&wi->path) < 0 ||
		git_ignore__for_path(repo, "", &wi->ignores) < 0)
	{
		git__free(wi);
		return -1;
	}
	wi->root_len = wi->path.size;

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

int git_iterator_set_ignore_case(git_iterator *iter, bool ignore_case)
{
	bool desire_ignore_case  = (ignore_case != 0);

	if (iterator__ignore_case(iter) == desire_ignore_case)
		return 0;

	if (iter->type == GIT_ITERATOR_TYPE_EMPTY) {
		if (desire_ignore_case)
			iter->flags |= GIT_ITERATOR_IGNORE_CASE;
		else
			iter->flags &= ~GIT_ITERATOR_IGNORE_CASE;
	} else {
		giterr_set(GITERR_INVALID,
			"Cannot currently set ignore case on non-empty iterators");
		return -1;
	}

	return 0;
}

git_index *git_iterator_get_index(git_iterator *iter)
{
	if (iter->type == GIT_ITERATOR_TYPE_INDEX)
		return ((index_iterator *)iter)->index;
	return NULL;
}

int git_iterator_current_tree_entry(
	const git_tree_entry **tree_entry, git_iterator *iter)
{
	if (iter->type != GIT_ITERATOR_TYPE_TREE)
		*tree_entry = NULL;
	else {
		tree_iterator_frame *tf = ((tree_iterator *)iter)->head;
		*tree_entry = (tf->current < tf->n_entries) ?
			tf->entries[tf->current]->te : NULL;
	}

	return 0;
}

int git_iterator_current_parent_tree(
	const git_tree **tree_ptr,
	git_iterator *iter,
	const char *parent_path)
{
	tree_iterator *ti = (tree_iterator *)iter;
	tree_iterator_frame *tf;
	const char *scan = parent_path;
	const git_tree_entry *te;

	*tree_ptr = NULL;

	if (iter->type != GIT_ITERATOR_TYPE_TREE)
		return 0;

	for (tf = ti->root; *scan; ) {
		if (!(tf = tf->down) ||
			tf->current >= tf->n_entries ||
			!(te = tf->entries[tf->current]->te) ||
			ti->strncomp(scan, te->filename, te->filename_len) != 0)
			return 0;

		scan += te->filename_len;
		if (*scan == '/')
			scan++;
	}

	*tree_ptr = tf->entries[tf->current]->tree;
	return 0;
}

bool git_iterator_current_is_ignored(git_iterator *iter)
{
	workdir_iterator *wi = (workdir_iterator *)iter;

	if (iter->type != GIT_ITERATOR_TYPE_WORKDIR)
		return false;

	if (wi->is_ignored != -1)
		return (bool)(wi->is_ignored != 0);

	if (git_ignore__lookup(&wi->ignores, wi->entry.path, &wi->is_ignored) < 0)
		wi->is_ignored = true;

	return (bool)wi->is_ignored;
}

int git_iterator_cmp(git_iterator *iter, const char *path_prefix)
{
	const git_index_entry *entry;

	/* a "done" iterator is after every prefix */
	if (git_iterator_current(&entry, iter) < 0 || entry == NULL)
		return 1;

	/* a NULL prefix is after any valid iterator */
	if (!path_prefix)
		return -1;

	return iter->prefixcomp(entry->path, path_prefix);
}

int git_iterator_current_workdir_path(git_buf **path, git_iterator *iter)
{
	workdir_iterator *wi = (workdir_iterator *)iter;

	if (iter->type != GIT_ITERATOR_TYPE_WORKDIR || !wi->entry.path)
		*path = NULL;
	else
		*path = &wi->path;

	return 0;
}
