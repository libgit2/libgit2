/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_attrcache_h__
#define INCLUDE_attrcache_h__

#include "pool.h"
#include "strmap.h"
#include "buffer.h"

#define GIT_ATTR_CONFIG       "core.attributesfile"
#define GIT_IGNORE_CONFIG     "core.excludesfile"

typedef struct {
	char *cfg_attr_file; /* cached value of core.attributesfile */
	char *cfg_excl_file; /* cached value of core.excludesfile */
	git_strmap *files;	 /* hash path to git_attr_cache_entry records */
	git_strmap *macros;	 /* hash name to vector<git_attr_assignment> */
	git_mutex lock;
	git_pool  pool;
} git_attr_cache;

extern int git_attr_cache__init(git_repository *repo);

typedef enum {
	GIT_ATTR_CACHE__FROM_FILE = 0,
	GIT_ATTR_CACHE__FROM_INDEX = 1,

	GIT_ATTR_CACHE_NUM_SOURCES = 2
} git_attr_cache_source;

typedef struct git_attr_file git_attr_file;
typedef struct git_attr_rule git_attr_rule;

typedef struct {
	git_attr_file *file[GIT_ATTR_CACHE_NUM_SOURCES];
	const char *path; /* points into fullpath */
	char fullpath[GIT_FLEX_ARRAY];
} git_attr_cache_entry;

typedef int (*git_attr_cache_parser)(
	git_repository *repo,
	git_attr_file *file,
	const char *data,
	void *payload);

/* get file - loading and reload as needed */
extern int git_attr_cache__get(
	git_attr_file **file,
	git_repository *repo,
	git_attr_cache_source source,
	const char *base,
	const char *filename,
	git_attr_cache_parser parser,
	void *payload);

extern bool git_attr_cache__is_cached(
	git_repository *repo,
	git_attr_cache_source source,
	const char *path);

extern int git_attr_cache__insert_macro(
	git_repository *repo, git_attr_rule *macro);

extern git_attr_rule *git_attr_cache__lookup_macro(
	git_repository *repo, const char *name);

extern int git_attr_cache_entry__new(
	git_attr_cache_entry **out,
	const char *base,
	const char *path,
	git_pool *pool);

#endif
