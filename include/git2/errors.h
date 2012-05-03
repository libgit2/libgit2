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

typedef enum {
	GIT_SUCCESS = 0,
	GIT_ERROR = -1,

	/** Input does not exist in the scope searched. */
	GIT_ENOTFOUND = -3,

	/** A reference with this name already exists */
	GIT_EEXISTS = -23,

	/** The given integer literal is too large to be parsed */
	GIT_EOVERFLOW = -24,

	/** The given short oid is ambiguous */
	GIT_EAMBIGUOUS = -29,

	/** Skip and passthrough the given ODB backend */
	GIT_EPASSTHROUGH = -30,

	/** The buffer is too short to satisfy the request */
	GIT_ESHORTBUFFER = -32,

	GIT_EREVWALKOVER = -33,
} git_error_t;

typedef struct {
	char *message;
	int klass;
} git_error;

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
} git_error_class;

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
