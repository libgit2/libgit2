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

typedef struct {
	int initialized;
	git_pool pool;
	git_strmap *files;	 /* hash path to git_attr_file of rules */
	git_strmap *macros;	 /* hash name to vector<git_attr_assignment> */
	char *cfg_attr_file; /* cached value of core.attributesfile */
	char *cfg_excl_file; /* cached value of core.excludesfile */
} git_attr_cache;

extern int git_attr_cache__init(git_repository *repo);

#endif
