/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "pathspec.h"
#include "buf_text.h"
#include "attr_file.h"

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
bool git_pathspec_is_interesting(const git_strarray *pathspec)
{
	const char *str;

	if (pathspec == NULL || pathspec->count == 0)
		return false;
	if (pathspec->count > 1)
		return true;

	str = pathspec->strings[0];
	if (!str || !str[0] || (!str[1] && (str[0] == '*' || str[0] == '.')))
		return false;
	return true;
}

/* build a vector of fnmatch patterns to evaluate efficiently */
int git_pathspec_init(
	git_vector *vspec, const git_strarray *strspec, git_pool *strpool)
{
	size_t i;

	memset(vspec, 0, sizeof(*vspec));

	if (!git_pathspec_is_interesting(strspec))
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
void git_pathspec_free(git_vector *vspec)
{
	git_attr_fnmatch *match;
	unsigned int i;

	git_vector_foreach(vspec, i, match) {
		git__free(match);
		vspec->contents[i] = NULL;
	}

	git_vector_free(vspec);
}

/* match a path against the vectorized pathspec */
bool git_pathspec_match_path(
	git_vector *vspec,
	const char *path,
	bool disable_fnmatch,
	bool casefold,
	const char **matched_pathspec)
{
	size_t i;
	git_attr_fnmatch *match;
	int fnmatch_flags = 0;
	int (*use_strcmp)(const char *, const char *);
	int (*use_strncmp)(const char *, const char *, size_t);

	if (matched_pathspec)
		*matched_pathspec = NULL;

	if (!vspec || !vspec->length)
		return true;

	if (disable_fnmatch)
		fnmatch_flags = -1;
	else if (casefold)
		fnmatch_flags = FNM_CASEFOLD;

	if (casefold) {
		use_strcmp  = git__strcasecmp;
		use_strncmp = git__strncasecmp;
	} else {
		use_strcmp  = git__strcmp;
		use_strncmp = git__strncmp;
	}

	git_vector_foreach(vspec, i, match) {
		int result = use_strcmp(match->pattern, path) ? FNM_NOMATCH : 0;

		if (fnmatch_flags >= 0 && result == FNM_NOMATCH)
			result = p_fnmatch(match->pattern, path, fnmatch_flags);

		/* if we didn't match, look for exact dirname prefix match */
		if (result == FNM_NOMATCH &&
			(match->flags & GIT_ATTR_FNMATCH_HASWILD) == 0 &&
			use_strncmp(path, match->pattern, match->length) == 0 &&
			path[match->length] == '/')
			result = 0;

		if (result == 0) {
			if (matched_pathspec)
				*matched_pathspec = match->pattern;

			return (match->flags & GIT_ATTR_FNMATCH_NEGATIVE) ? false : true;
		}
	}

	return false;
}

