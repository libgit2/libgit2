/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#ifndef ALLOC_H
#define ALLOC_H

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define git__malloc  gitbench__malloc
#define git__calloc  gitbench__calloc
#define git__realloc gitbench__realloc
#define git__free    gitbench__free
#define git__strdup  gitbench__strdup

typedef struct gitbench_alloc_stat_t {
	size_t total_alloc_count;
	size_t total_dealloc_count;
	size_t total_alloc_current;
	size_t total_alloc_max;

	size_t run_alloc_count;
	size_t run_dealloc_count;
	size_t run_alloc_current;
	size_t run_alloc_max;
} gitbench_alloc_stat_t;

extern int gitbench_alloc_init(void);
extern void gitbench_alloc_start(void);
extern void gitbench_alloc_stop(void);
extern gitbench_alloc_stat_t *gitbench_alloc_stats(void);
extern void gitbench_alloc_shutdown(void);

extern void *gitbench__malloc(size_t len);
extern void *gitbench__calloc(size_t nelem, size_t elsize);
extern void *gitbench__realloc(void *ptr, size_t size);
extern void gitbench__free(void *ptr);
extern char *gitbench__strdup(const char *str);

#endif
