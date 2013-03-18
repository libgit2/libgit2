/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_attr_h__
#define INCLUDE_attr_h__

#include "attr_file.h"

#define GIT_ATTR_CONFIG       "core.attributesfile"
#define GIT_IGNORE_CONFIG     "core.excludesfile"

typedef int (*git_attr_file_parser)(
	git_repository *, void *, const char *, git_attr_file *);

extern int git_attr_cache__insert_macro(
	git_repository *repo, git_attr_rule *macro);

extern git_attr_rule *git_attr_cache__lookup_macro(
	git_repository *repo, const char *name);

extern int git_attr_cache__push_file(
	git_repository *repo,
	const char *base,
	const char *filename,
	git_attr_file_source source,
	git_attr_file_parser parse,
	void *parsedata, /* passed through to parse function */
	git_vector *stack);

extern int git_attr_cache__internal_file(
	git_repository *repo,
	const char *key,
	git_attr_file **file_ptr);

/* returns true if path is in cache */
extern bool git_attr_cache__is_cached(
	git_repository *repo, git_attr_file_source source, const char *path);

extern int git_attr_cache__decide_sources(
	uint32_t flags, bool has_wd, bool has_index, git_attr_file_source *srcs);

#endif
