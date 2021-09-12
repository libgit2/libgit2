/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_email_h__
#define INCLUDE_git_email_h__

#include "common.h"

/**
 * @file git2/email.h
 * @brief Git email formatting and application routines.
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * Formatting options for diff e-mail generation
 */
typedef enum {
	/** Normal patch, the default */
	GIT_EMAIL_CREATE_DEFAULT = 0,

	/** Do not include patch numbers in the subject prefix. */
	GIT_EMAIL_CREATE_OMIT_NUMBERS = (1u << 0),

	/**
	 * Include numbers in the subject prefix even when the
	 * patch is for a single commit (1/1).
	 */
	GIT_EMAIL_CREATE_ALWAYS_NUMBER = (1u << 1),
} git_email_create_flags_t;

/**
 * Options for controlling the formatting of the generated e-mail.
 */
typedef struct {
	unsigned int version;

	/** see `git_email_create_flags_t` above */
	uint32_t flags;

	/** Options to use when creating diffs */
	git_diff_options diff_opts;

	/**
	 * The subject prefix, by default "PATCH".  If set to an empty
	 * string ("") then only the patch numbers will be shown in the
	 * prefix.  If the subject_prefix is empty and patch numbers
	 * are not being shown, the prefix will be omitted entirely.
	 */
	const char *subject_prefix;

	/**
	 * The starting patch number; this cannot be 0.  By default,
	 * this is 1.
	 */
	size_t start_number;

	/** The "re-roll" number.  By default, there is no re-roll. */
	size_t reroll_number;
} git_email_create_options;

#define GIT_EMAIL_CREATE_OPTIONS_VERSION 1
#define GIT_EMAIL_CREATE_OPTIONS_INIT { \
		GIT_EMAIL_CREATE_OPTIONS_VERSION, \
		GIT_EMAIL_CREATE_DEFAULT, \
		GIT_DIFF_OPTIONS_INIT \
	}

/**
 * Create a diff for a commit in mbox format for sending via email.
 * The commit must not be a merge commit.
 *
 * @param out buffer to store the e-mail patch in
 * @param commit commit to create a patch for
 * @param opts email creation options
 */
GIT_EXTERN(int) git_email_create_from_commit(
	git_buf *out,
	git_commit *commit,
	const git_email_create_options *opts);

/**
 * Create an mbox format diff for the given commits in the revision
 * spec, excluding merge commits.
 *
 * @param out buffer to store the e-mail patches in
 * @param commits the array of commits to create patches for
 * @param len the length of the `commits` array
 * @param opts email creation options
 */
GIT_EXTERN(int) git_email_create_from_commits(
	git_strarray *out,
	git_commit **commits,
	size_t len,
	const git_email_create_options *opts);

GIT_END_DECL

/** @} */

#endif
