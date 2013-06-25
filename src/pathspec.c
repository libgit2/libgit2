/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "git2/pathspec.h"
#include "pathspec.h"
#include "buf_text.h"
#include "attr_file.h"
#include "iterator.h"
#include "repository.h"
#include "index.h"

/* what is the common non-wildcard prefix for all items in the pathspec */
char *git_pathspec_prefix(const git_strarray *pathspec)
{
	git_buf prefix = GIT_BUF_INIT;
	const char *scan;

	if (!pathspec || !pathspec->count ||
		git_buf_text_common_prefix(&prefix, pathspec) < 0)
		return NULL;

	/* diff prefix will only be leading non-wildcards */
	for (scan = prefix.ptr; *scan; ++scan) {
		if (git__iswildcard(*scan) &&
			(scan == prefix.ptr || (*(scan - 1) != '\\')))
			break;
	}
	git_buf_truncate(&prefix, scan - prefix.ptr);

	if (prefix.size <= 0) {
		git_buf_free(&prefix);
		return NULL;
	}

	git_buf_text_unescape(&prefix);

	return git_buf_detach(&prefix);
}

/* is there anything in the spec that needs to be filtered on */
bool git_pathspec_is_empty(const git_strarray *pathspec)
{
	size_t i;

	if (pathspec == NULL)
		return true;

	for (i = 0; i < pathspec->count; ++i) {
		const char *str = pathspec->strings[i];

		if (str && str[0])
			return false;
	}

	return true;
}

/* build a vector of fnmatch patterns to evaluate efficiently */
int git_pathspec__vinit(
	git_vector *vspec, const git_strarray *strspec, git_pool *strpool)
{
	size_t i;

	memset(vspec, 0, sizeof(*vspec));

	if (git_pathspec_is_empty(strspec))
		return 0;

	if (git_vector_init(vspec, strspec->count, NULL) < 0)
		return -1;

	for (i = 0; i < strspec->count; ++i) {
		int ret;
		const char *pattern = strspec->strings[i];
		git_attr_fnmatch *match = git__calloc(1, sizeof(git_attr_fnmatch));
		if (!match)
			return -1;

		match->flags = GIT_ATTR_FNMATCH_ALLOWSPACE;

		ret = git_attr_fnmatch__parse(match, strpool, NULL, &pattern);
		if (ret == GIT_ENOTFOUND) {
			git__free(match);
			continue;
		} else if (ret < 0)
			return ret;

		if (git_vector_insert(vspec, match) < 0)
			return -1;
	}

	return 0;
}

/* free data from the pathspec vector */
void git_pathspec__vfree(git_vector *vspec)
{
	git_attr_fnmatch *match;
	unsigned int i;

	git_vector_foreach(vspec, i, match) {
		git__free(match);
		vspec->contents[i] = NULL;
	}

	git_vector_free(vspec);
}

struct pathspec_match_context {
	int fnmatch_flags;
	int (*strcomp)(const char *, const char *);
	int (*strncomp)(const char *, const char *, size_t);
};

static void pathspec_match_context_init(
	struct pathspec_match_context *ctxt,
	bool disable_fnmatch,
	bool casefold)
{
	if (disable_fnmatch)
		ctxt->fnmatch_flags = -1;
	else if (casefold)
		ctxt->fnmatch_flags = FNM_CASEFOLD;
	else
		ctxt->fnmatch_flags = 0;

	if (casefold) {
		ctxt->strcomp  = git__strcasecmp;
		ctxt->strncomp = git__strncasecmp;
	} else {
		ctxt->strcomp  = git__strcmp;
		ctxt->strncomp = git__strncmp;
	}
}

static int pathspec_match_one(
	const git_attr_fnmatch *match,
	struct pathspec_match_context *ctxt,
	const char *path)
{
	int result = (match->flags & GIT_ATTR_FNMATCH_MATCH_ALL) ? 0 : FNM_NOMATCH;

	if (result == FNM_NOMATCH)
		result = ctxt->strcomp(match->pattern, path) ? FNM_NOMATCH : 0;

	if (ctxt->fnmatch_flags >= 0 && result == FNM_NOMATCH)
		result = p_fnmatch(match->pattern, path, ctxt->fnmatch_flags);

	/* if we didn't match, look for exact dirname prefix match */
	if (result == FNM_NOMATCH &&
		(match->flags & GIT_ATTR_FNMATCH_HASWILD) == 0 &&
		ctxt->strncomp(path, match->pattern, match->length) == 0 &&
		path[match->length] == '/')
		result = 0;

	if (result == 0)
		return (match->flags & GIT_ATTR_FNMATCH_NEGATIVE) ? 0 : 1;
	return -1;
}

/* match a path against the vectorized pathspec */
bool git_pathspec__match(
	const git_vector *vspec,
	const char *path,
	bool disable_fnmatch,
	bool casefold,
	const char **matched_pathspec,
	size_t *matched_at)
{
	size_t i;
	const git_attr_fnmatch *match;
	struct pathspec_match_context ctxt;

	if (matched_pathspec)
		*matched_pathspec = NULL;
	if (matched_at)
		*matched_at = GIT_PATHSPEC_NOMATCH;

	if (!vspec || !vspec->length)
		return true;

	pathspec_match_context_init(&ctxt, disable_fnmatch, casefold);

	git_vector_foreach(vspec, i, match) {
		int result = pathspec_match_one(match, &ctxt, path);

		if (result >= 0) {
			if (matched_pathspec)
				*matched_pathspec = match->pattern;
			if (matched_at)
				*matched_at = i;

			return (result != 0);
		}
	}

	return false;
}


int git_pathspec__init(git_pathspec *ps, const git_strarray *paths)
{
	int error = 0;

	memset(ps, 0, sizeof(*ps));

	ps->prefix = git_pathspec_prefix(paths);

	if ((error = git_pool_init(&ps->pool, 1, 0)) < 0 ||
		(error = git_pathspec__vinit(&ps->pathspec, paths, &ps->pool)) < 0)
		git_pathspec__clear(ps);

	return error;
}

void git_pathspec__clear(git_pathspec *ps)
{
	git__free(ps->prefix);
	git_pathspec__vfree(&ps->pathspec);
	git_pool_clear(&ps->pool);
	memset(ps, 0, sizeof(*ps));
}

int git_pathspec_new(git_pathspec **out, const git_strarray *pathspec)
{
	int error = 0;
	git_pathspec *ps = git__malloc(sizeof(git_pathspec));
	GITERR_CHECK_ALLOC(ps);

	if ((error = git_pathspec__init(ps, pathspec)) < 0) {
		git__free(ps);
		return error;
	}

	GIT_REFCOUNT_INC(ps);
	*out = ps;
	return 0;
}

static void pathspec_free(git_pathspec *ps)
{
	git_pathspec__clear(ps);
	git__free(ps);
}

void git_pathspec_free(git_pathspec *ps)
{
	if (!ps)
		return;
	GIT_REFCOUNT_DEC(ps, pathspec_free);
}

int git_pathspec_matches_path(
	const git_pathspec *ps, uint32_t flags, const char *path)
{
	bool no_fnmatch = (flags & GIT_PATHSPEC_NO_GLOB) != 0;
	bool casefold =  (flags & GIT_PATHSPEC_IGNORE_CASE) != 0;

	assert(ps && path);

	return (0 != git_pathspec__match(
		&ps->pathspec, path, no_fnmatch, casefold, NULL, NULL));
}

static void pathspec_match_free(git_pathspec_match_list *m)
{
	git_pathspec_free(m->pathspec);
	m->pathspec = NULL;

	git_array_clear(m->matches);
	git_array_clear(m->failures);
	git_pool_clear(&m->pool);
	git__free(m);
}

static git_pathspec_match_list *pathspec_match_alloc(git_pathspec *ps)
{
	git_pathspec_match_list *m = git__calloc(1, sizeof(git_pathspec_match_list));

	if (m != NULL && git_pool_init(&m->pool, 1, 0) < 0) {
		pathspec_match_free(m);
		m = NULL;
	}

	/* need to keep reference to pathspec and increment refcount because
	 * failures array stores pointers to the pattern strings of the
	 * pathspec that had no matches
	 */
	GIT_REFCOUNT_INC(ps);
	m->pathspec = ps;

	return m;
}

GIT_INLINE(void) pathspec_mark_pattern(uint8_t *used, size_t pos, size_t *ct)
{
	if (!used[pos]) {
		used[pos] = 1;
		(*ct)++;
	}
}

static int pathspec_match_from_iterator(
	git_pathspec_match_list **out,
	git_iterator *iter,
	uint32_t flags,
	git_pathspec *ps)
{
	int error = 0;
	git_pathspec_match_list *m;
	const git_index_entry *entry = NULL;
	struct pathspec_match_context ctxt;
	git_vector *patterns = &ps->pathspec;
	bool find_failures = (flags & GIT_PATHSPEC_FIND_FAILURES) != 0;
	bool failures_only = (flags & GIT_PATHSPEC_FAILURES_ONLY) != 0;
	size_t pos, used_ct = 0, found_files = 0;
	git_index *index = NULL;
	uint8_t *used_patterns = NULL;
	char **file;

	*out = m = pathspec_match_alloc(ps);
	GITERR_CHECK_ALLOC(m);

	if ((error = git_iterator_reset(iter, ps->prefix, ps->prefix)) < 0)
		goto done;

	if (patterns->length > 0) {
		used_patterns = git__calloc(patterns->length, sizeof(uint8_t));
		GITERR_CHECK_ALLOC(used_patterns);
	}

	if (git_iterator_type(iter) == GIT_ITERATOR_TYPE_WORKDIR &&
		(error = git_repository_index__weakptr(
			&index, git_iterator_owner(iter))) < 0)
		goto done;

	pathspec_match_context_init(&ctxt,
		(flags & GIT_PATHSPEC_NO_GLOB) != 0, git_iterator_ignore_case(iter));

	while (!(error = git_iterator_advance(&entry, iter))) {
		int result = -1;

		for (pos = 0; pos < patterns->length; ++pos) {
			const git_attr_fnmatch *pat = git_vector_get(patterns, pos);

			result = pathspec_match_one(pat, &ctxt, entry->path);
			if (result >= 0)
				break;
		}

		/* no matches for this path */
		if (result < 0)
			continue;

		/* if result was a negative pattern match, then don't list file */
		if (!result) {
			pathspec_mark_pattern(used_patterns, pos, &used_ct);
			continue;
		}

		/* check if path is untracked and ignored */
		if (index != NULL &&
			git_iterator_current_is_ignored(iter) &&
			git_index__find(NULL, index, entry->path, GIT_INDEX_STAGE_ANY) < 0)
			continue;

		/* mark the matched pattern as used */
		pathspec_mark_pattern(used_patterns, pos, &used_ct);
		++found_files;

		/* if find_failures is on, check if any later patterns also match */
		if (find_failures && used_ct < patterns->length) {
			for (++pos; pos < patterns->length; ++pos) {
				const git_attr_fnmatch *pat = git_vector_get(patterns, pos);
				if (used_patterns[pos])
					continue;

				if (pathspec_match_one(pat, &ctxt, entry->path) > 0)
					pathspec_mark_pattern(used_patterns, pos, &used_ct);
			}
		}

		/* if only looking at failures, exit early or just continue */
		if (failures_only) {
			if (used_ct == patterns->length)
				break;
			continue;
		}

		/* insert matched path into matches array */
		if ((file = git_array_alloc(m->matches)) == NULL ||
			(*file = git_pool_strdup(&m->pool, entry->path)) == NULL) {
			error = -1;
			goto done;
		}
	}

	if (error < 0 && error != GIT_ITEROVER)
		goto done;
	error = 0;

	/* insert patterns that had no matches into failures array */
	if (find_failures && used_ct < patterns->length) {
		for (pos = 0; pos < patterns->length; ++pos) {
			const git_attr_fnmatch *pat = git_vector_get(patterns, pos);
			if (used_patterns[pos])
				continue;

			if ((file = git_array_alloc(m->failures)) == NULL ||
				(*file = git_pool_strdup(&m->pool, pat->pattern)) == NULL) {
				error = -1;
				goto done;
			}
		}
	}

	/* if every pattern failed to match, then we have failed */
	if ((flags & GIT_PATHSPEC_NO_MATCH_ERROR) != 0 && !found_files) {
		giterr_set(GITERR_INVALID, "No matching files were found");
		error = GIT_ENOTFOUND;
	}

done:
	git__free(used_patterns);

	if (error < 0) {
		pathspec_match_free(m);
		*out = NULL;
	}

	return error;
}

static git_iterator_flag_t pathspec_match_iter_flags(uint32_t flags)
{
	git_iterator_flag_t f = 0;

	if ((flags & GIT_PATHSPEC_IGNORE_CASE) != 0)
		f |= GIT_ITERATOR_IGNORE_CASE;
	else if ((flags & GIT_PATHSPEC_USE_CASE) != 0)
		f |= GIT_ITERATOR_DONT_IGNORE_CASE;

	return f;
}

int git_pathspec_match_workdir(
	git_pathspec_match_list **out,
	git_repository *repo,
	uint32_t flags,
	git_pathspec *ps)
{
	int error = 0;
	git_iterator *iter;

	assert(out && repo);

	if (!(error = git_iterator_for_workdir(
			&iter, repo, pathspec_match_iter_flags(flags), NULL, NULL))) {

		error = pathspec_match_from_iterator(out, iter, flags, ps);

		git_iterator_free(iter);
	}

	return error;
}

int git_pathspec_match_index(
	git_pathspec_match_list **out,
	git_index *index,
	uint32_t flags,
	git_pathspec *ps)
{
	int error = 0;
	git_iterator *iter;

	assert(out && index);

	if (!(error = git_iterator_for_index(
			&iter, index, pathspec_match_iter_flags(flags), NULL, NULL))) {

		error = pathspec_match_from_iterator(out, iter, flags, ps);

		git_iterator_free(iter);
	}

	return error;
}

int git_pathspec_match_tree(
	git_pathspec_match_list **out,
	git_tree *tree,
	uint32_t flags,
	git_pathspec *ps)
{
	int error = 0;
	git_iterator *iter;

	assert(out && tree);

	if (!(error = git_iterator_for_tree(
			&iter, tree, pathspec_match_iter_flags(flags), NULL, NULL))) {

		error = pathspec_match_from_iterator(out, iter, flags, ps);

		git_iterator_free(iter);
	}

	return error;
}

void git_pathspec_match_list_free(git_pathspec_match_list *m)
{
	pathspec_match_free(m);
}

size_t git_pathspec_match_list_entrycount(
	const git_pathspec_match_list *m)
{
	return git_array_size(m->matches);
}

const char *git_pathspec_match_list_entry(
	const git_pathspec_match_list *m, size_t pos)
{
	char **entry = git_array_get(m->matches, pos);
	return entry ? *entry : NULL;
}

size_t git_pathspec_match_list_failed_entrycount(
	const git_pathspec_match_list *m)
{
	return git_array_size(m->failures);
}

const char * git_pathspec_match_list_failed_entry(
	const git_pathspec_match_list *m, size_t pos)
{
	char **entry = git_array_get(m->failures, pos);
	return entry ? *entry : NULL;
}
