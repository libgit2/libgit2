/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_signature_h__
#define INCLUDE_git_signature_h__

#include "common.h"
#include "types.h"

/**
 * @file git2/signature.h
 * @brief Git signature creation
 * @defgroup git_signature Git signature creation
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * Create a new action signature.
 *
 * Call `git_signature_free()` to free the data.
 *
 * Note: angle brackets ('<' and '>') characters are not allowed
 * to be used in either the `name` or the `email` parameter.
 *
 * @param out new signature, in case of error NULL
 * @param name name of the person
 * @param email email of the person
 * @param time time (in seconds from epoch) when the action happened
 * @param offset timezone offset (in minutes) for the time
 * @return 0 or an error code
 */
GIT_EXTERN(int) git_signature_new(git_signature **out, const char *name, const char *email, git_time_t time, int offset);

/**
 * Create a new action signature with a timestamp of 'now'.
 *
 * Call `git_signature_free()` to free the data.
 *
 * @param out new signature, in case of error NULL
 * @param name name of the person
 * @param email email of the person
 * @return 0 or an error code
 */
GIT_EXTERN(int) git_signature_now(git_signature **out, const char *name, const char *email);

/** Create a new author action signature with default information based on the
 * configuration and environment variables.
 *
 * If GIT_AUTHOR_NAME environment variable is set it uses that over the
 * user.name value from the configuration.
 *
 * If GIT_AUTHOR_EMAIL environment variable is set it uses that over the
 * user.email value from the configuration.  The EMAIL environment variable is
 * the fallback email address in case the user.email configuration value isn't
 * set.
 *
 * If GIT_AUTHOR_DATE is set it uses that, otherwise it uses the current time
 * as the timestamp.
 *
 * It will return GIT_ENOTFOUND if either the user.name or user.email are not
 * set and there is no fallback from an environment variable.
 *
 * @param out new signature
 * @param repo repository pointer
 * @return 0 on success, GIT_ENOTFOUND if config is missing, or error code
 */
GIT_EXTERN(int) git_signature_default_author(git_signature **out, git_repository *repo);

/** Create a new committer action signature with default information based on
 * the configuration and environment variables.
 *
 * If GIT_COMMITTER_NAME environment variable is set it uses that over the
 * user.name value from the configuration.
 *
 * If GIT_COMMITTER_EMAIL environment variable is set it uses that over the
 * user.email value from the configuration.  The EMAIL environment variable is
 * the fallback email address in case the user.email configuration value isn't
 * set.
 *
 * If GIT_COMMITTER_DATE is set it uses that, otherwise it uses the current
 * time as the timestamp.
 *
 * It will return GIT_ENOTFOUND if either the user.name or user.email are not
 * set and there is no fallback from an environment variable.
 *
 * @param out new signature @param repo repository pointer @return 0 on
 * success, GIT_ENOTFOUND if config is missing, or error code
 */
GIT_EXTERN(int) git_signature_default_committer(git_signature **out, git_repository *repo);

#ifndef GIT_DEPRECATE_HARD
/**
 * Create a new action signature with default user and now timestamp.
 *
 * This looks up the user.name and user.email from the configuration and
 * uses the current time as the timestamp, and creates a new signature
 * based on that information.  It will return GIT_ENOTFOUND if either the
 * user.name or user.email are not set.
 *
 * @deprecated use git_signature_default_author or git_signature_default_committer instead
 * @param out new signature
 * @param repo repository pointer
 * @return 0 on success, GIT_ENOTFOUND if config is missing, or error code
 */
GIT_EXTERN(int) git_signature_default(git_signature **out, git_repository *repo);
#endif

/**
 * Create a new signature by parsing the given buffer, which is
 * expected to be in the format "Real Name <email> timestamp tzoffset",
 * where `timestamp` is the number of seconds since the Unix epoch and
 * `tzoffset` is the timezone offset in `hhmm` format (note the lack
 * of a colon separator).
 *
 * @param out new signature
 * @param buf signature string
 * @return 0 on success, GIT_EINVALID if the signature is not parseable, or an error code
 */
GIT_EXTERN(int) git_signature_from_buffer(git_signature **out, const char *buf);

/**
 * Create a copy of an existing signature.  All internal strings are also
 * duplicated.
 *
 * Call `git_signature_free()` to free the data.
 *
 * @param dest pointer where to store the copy
 * @param sig signature to duplicate
 * @return 0 or an error code
 */
GIT_EXTERN(int) git_signature_dup(git_signature **dest, const git_signature *sig);

/**
 * Free an existing signature.
 *
 * Because the signature is not an opaque structure, it is legal to free it
 * manually, but be sure to free the "name" and "email" strings in addition
 * to the structure itself.
 *
 * @param sig signature to free
 */
GIT_EXTERN(void) git_signature_free(git_signature *sig);

/** @} */
GIT_END_DECL
#endif
