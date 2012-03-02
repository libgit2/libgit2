/*
 * Copyright (C) 2009-2012 the libgit2 contributors
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

#define GIT_ATTR_TRUE(attr)		((attr) == git_attr__true)
#define GIT_ATTR_FALSE(attr)	((attr) == git_attr__false)
#define GIT_ATTR_UNSPECIFIED(attr)	((attr) == NULL)

GIT_EXTERN(const char *) git_attr__true;
GIT_EXTERN(const char *) git_attr__false;


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

/**
 * Flush the gitattributes cache.
 *
 * Call this if you have reason to believe that the attributes files
 * on disk no longer match the cached contents of memory.
 */
GIT_EXTERN(void) git_attr_cache_flush(
	git_repository *repo);

/**
 * Add a macro definition.
 *
 * Macros will automatically be loaded from the top level .gitattributes
 * file of the repository (plus the build-in "binary" macro).  This
 * function allows you to add others.  For example, to add the default
 * macro, you would call:
 *
 *    git_attr_add_macro(repo, "binary", "-diff -crlf");
 */
GIT_EXTERN(int) git_attr_add_macro(
	git_repository *repo,
	const char *name,
	const char *values);

/** @} */
GIT_END_DECL
#endif

