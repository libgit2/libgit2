/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_mailmap_h__
#define INCLUDE_git_mailmap_h__

#include "common.h"
#include "types.h"

/**
 * @file git2/mailmap.h
 * @brief Mailmap parsing routines
 * @defgroup git_mailmap Git mailmap routines
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * A single entry parsed from a mailmap.
 */
typedef struct git_mailmap_entry {
	unsigned int version;

	const char *real_name; /**< the real name (may be NULL) */
	const char *real_email; /**< the real email (may be NULL) */
	const char *replace_name; /**< the name to replace (may be NULL) */
	const char *replace_email; /**< the email to replace */
} git_mailmap_entry;

#define GIT_MAILMAP_ENTRY_VERSION 1
#define GIT_MAILMAP_ENTRY_INIT {GIT_MAILMAP_ENTRY_VERSION}

/**
 * Create a mailmap object by parsing a mailmap file.
 *
 * The mailmap must be freed with 'git_mailmap_free'.
 *
 * @param out pointer to store the mailmap
 * @param data raw data buffer to parse
 * @param size size of the raw data buffer
 * @return 0 on success
 */
GIT_EXTERN(int) git_mailmap_parse(
	git_mailmap **out,
	const char *data,
	size_t size);

/**
 * Create a mailmap object from the given repository.
 *
 * If the repository is not bare, the repository's working directory root will
 * be checked for the '.mailmap' file to be parsed.
 *
 * If the repository is bare, the repository's HEAD commit's tree root will be
 * searched for the '.mailmap' file to be parsed.
 *
 * The mailmap must be freed with 'git_mailmap_free'.
 *
 * @param out pointer to store the mailmap
 * @param repo repository to find the .mailmap in
 * @return 0 on success; GIT_ENOTFOUND if .mailmap could not be found.
 */
GIT_EXTERN(int) git_mailmap_from_repo(
	git_mailmap **out,
	git_repository *repo);

/**
 * Free a mailmap created by 'git_mailmap_parse' or 'git_mailmap_from_repo'.
 */
GIT_EXTERN(void) git_mailmap_free(git_mailmap *mailmap);

/**
 * Resolve a name and email to the corresponding real name and email.
 *
 * @param name_out either 'name', or the real name to use.
 *             You should NOT free this value.
 * @param email_out either 'email' or the real email to use,
 *             You should NOT free this value.
 * @param mailmap the mailmap to perform the lookup in. (may be NULL)
 * @param name the name to resolve.
 * @param email the email to resolve.
 */
GIT_EXTERN(void) git_mailmap_resolve(
	const char **name_out,
	const char **email_out,
	const git_mailmap *mailmap,
	const char *name,
	const char *email);

/**
 * Get the number of mailmap entries in this mailmap.
 */
GIT_EXTERN(size_t) git_mailmap_entry_count(const git_mailmap *mailmap);

/**
 * Lookup a mailmap entry by index.
 *
 * Do not free the mailmap entry, it is owned by the mailmap.
 *
 * @return the mailmap entry at index, or NULL if it cannot be found.
 */
GIT_EXTERN(const git_mailmap_entry *) git_mailmap_entry_byindex(
	const git_mailmap *mailmap,
	size_t idx);

/**
 * Lookup a mailmap entry by name/email pair.
 *
 * Do not free the mailmap entry, it is owned by the mailmap.
 *
 * @param mailmap the mailmap to perform the lookup in. (may be NULL)
 * @param name the name to perform the lookup with.
 * @param email the email to perform the lookup with.
 * @return the corresponding mailmap entry, or NULL if it cannot be found.
 */
GIT_EXTERN(const git_mailmap_entry *) git_mailmap_entry_lookup(
	const git_mailmap *mailmap,
	const char *name,
	const char *email);

/** @} */
GIT_END_DECL
#endif
