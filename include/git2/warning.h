/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git2_warning_h__
#define INCLUDE_git2_warning_h__

#include "common.h"

/**
 * @file git2/warning.h
 * @brief Warning managment
 *
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/* git warning type */
typedef enum {
	GIT_WARNING_OBJPARSE = 1,
	GIT_WARNING_CRLF = 2,
} git_warning_type_t;

#define GIT_WARNING_CLASS(type, code) ((type << 8) | code)
#define GIT_WARNING_TYPE(klass) (klass >> 8)
#define GIT_WARNING_CODE(klass) (klass & 0x00FF)
#define GIT_WARNING_ANY 0

/* classes of warnings */
enum {
	GIT_WARNING_OBJPARSE__INVALID_SIGNATURE = GIT_WARNING_CLASS(GIT_WARNING_OBJPARSE, 1),
	GIT_WARNING_OBJPARSE__INVALID_TIMEZONE = GIT_WARNING_CLASS(GIT_WARNING_OBJPARSE, 2),
	GIT_WARNING_OBJPARSE__MISSING_EMAIL = GIT_WARNING_CLASS(GIT_WARNING_OBJPARSE, 3),
	GIT_WARNING_OBJPARSE__UNTERMINATED_EMAIL = GIT_WARNING_CLASS(GIT_WARNING_OBJPARSE, 4),

	GIT_WARNING_CRLF__INVALID = GIT_WARNING_CLASS(GIT_WARNING_CRLF, 1),
};
typedef uint16_t git_warning_class;

/**
 * Specific information about a warning raised by libgit2.
 *
 * A warning class is composed of a warning type (ie. the subsystem that
 * encounter a problem) and code (an identifier for this specific problem).
 * This makes it easy to filter on subsystems of interest when registering a
 * callback.
 *
 * Context is a pointer to a subsystem-defined structure with more information
 * about the warning. Note that it might change depending on the code.
 */
typedef struct {
	/**
	 * The warning's class
	 */
	git_warning_class klass;

	/**
	 * A short description of the warning
	 */
	char *msg;

	/**
	 * The repository where the warning was raised.
	 * Might be not be available.
	 */
	git_repository *repo;

	/**
	 * The specific context for this warning.
	 *
	 * Dependent on the warning's class.
	 */
	void *context;
} git_warning;

/*
 * WIP: I'd be tempted to move those in their specific subsystem, so we don't
 * end-up cluttering this file with *all* warning contexts.
 */

/**
 * WIP: The context for *some* objparse warnings.
 */
typedef struct {
	git_oid *oid;
} git_warning_objparse_context;

/**
 * WIP: The context for *some* crlf problems.
 */
typedef struct {
	char *path;
} git_warning_crlf_context;

/** The generic type for a warning callback */
typedef int (*git_warning_cb)(const git_warning *warning, void *payload);

/** An opaque value describing a specific warning registration */
typedef void *git_warning_token;

/**
 * Register a warning callback.
 *
 * @param token The token corresponding to the registration
 * @param mask A mask of warnings this callback has interest in
 * @param cb The callback itself
 * @param payload A user-provided payload that will be passed along with the warning
 * @return 0 on success, a negative value on error.
 */
int git_warning_register(git_warning_token *token, int16_t mask, git_warning_cb cb, void *payload);

/**
 * Unregister a warning callback.
 *
 * @param token The token previously given by git_warning_register
 * @return 0 on success, GIT_ENOTFOUND if the token wasn't found.
 */
int git_warning_unregister(git_warning_token *token);

GIT_END_DECL

#endif
