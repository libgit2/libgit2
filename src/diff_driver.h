/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_diff_driver_h__
#define INCLUDE_diff_driver_h__

#include "common.h"

typedef struct git_diff_driver_registry git_diff_driver_registry;

git_diff_driver_registry *git_diff_driver_registry_new();
void git_diff_driver_registry_free(git_diff_driver_registry *);

typedef struct git_diff_driver git_diff_driver;

int git_diff_driver_lookup(git_diff_driver **, git_repository *, const char *);
void git_diff_driver_free(git_diff_driver *);

/* returns -1 meaning "unknown", 0 meaning not binary, 1 meaning binary */
int git_diff_driver_is_binary(git_diff_driver *);

/* returns -1 meaning "unknown", 0 meaning not binary, 1 meaning binary */
int git_diff_driver_content_is_binary(
	git_diff_driver *, const char *content, size_t content_len);

typedef long (*git_diff_find_context_fn)(
	const char *, long, char *, long, void *);

git_diff_find_context_fn git_diff_driver_find_content_fn(git_diff_driver *);

#endif
