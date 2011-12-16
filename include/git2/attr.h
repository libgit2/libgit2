/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_attr_h__
#define INCLUDE_git_attr_h__

#include "common.h"
#include "types.h"

/**
 * @file git2/attr.h
 * @brief Git attribute management routines
 * @defgroup git_attr Git attribute management routines
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

#define GIT_ATTR_TRUE			git_attr__true
#define GIT_ATTR_FALSE			git_attr__false
#define GIT_ATTR_UNSPECIFIED	NULL

GIT_EXTERN(const char *)git_attr__true;
GIT_EXTERN(const char *)git_attr__false;


/**
 * Lookup attribute for path returning string caller must free
 */
GIT_EXTERN(int) git_attr_get(
    git_repository *repo, const char *path, const char *name,
	const char **value);

/**
 * Lookup list of attributes for path, populating array of strings
 */
GIT_EXTERN(int) git_attr_get_many(
    git_repository *repo, const char *path,
    size_t num_attr, const char **names,
	const char **values);

/**
 * Perform an operation on each attribute of a path.
 */
GIT_EXTERN(int) git_attr_foreach(
    git_repository *repo, const char *path,
	int (*callback)(const char *name, const char *value, void *payload),
	void *payload);

/** @} */
GIT_END_DECL
#endif

