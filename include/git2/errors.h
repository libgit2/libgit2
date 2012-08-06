/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_errors_h__
#define INCLUDE_git_errors_h__

#include "common.h"

/**
 * @file git2/errors.h
 * @brief Git error handling routines and variables
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/** Generic return codes */
enum {
	GIT_OK = 0,
	GIT_ERROR = -1,
	GIT_ENOTFOUND = -3,
	GIT_EEXISTS = -4,
	GIT_EAMBIGUOUS = -5,
	GIT_EBUFS = -6,
	GIT_EUSER = -7,

	GIT_PASSTHROUGH = -30,
	GIT_REVWALKOVER = -31,
};

typedef struct {
	char *message;
	int klass;
} git_error;

/** Error classes */
typedef enum {
	GITERR_NOMEMORY,
	GITERR_OS,
	GITERR_INVALID,
	GITERR_REFERENCE,
	GITERR_ZLIB,
	GITERR_REPOSITORY,
	GITERR_CONFIG,
	GITERR_REGEX,
	GITERR_ODB,
	GITERR_INDEX,
	GITERR_OBJECT,
	GITERR_NET,
	GITERR_TAG,
	GITERR_TREE,
	GITERR_INDEXER,
	GITERR_SSL,
} git_error_t;

/**
 * Return the last `git_error` object that was generated for the
 * current thread or NULL if no error has occurred.
 *
 * @return A git_error object.
 */
GIT_EXTERN(const git_error *) giterr_last(void);

/**
 * Clear the last library error that occurred for this thread.
 */
GIT_EXTERN(void) giterr_clear(void);

/** @} */
GIT_END_DECL
#endif
