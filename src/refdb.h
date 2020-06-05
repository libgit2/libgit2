/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_refdb_h__
#define INCLUDE_refdb_h__

#include "common.h"

#include "git2/refdb.h"
#include "repository.h"

struct git_refdb {
	git_refcount rc;
	git_repository *repo;
	git_refdb_backend *backend;
};

void git_refdb__free(git_refdb *db);

int git_refdb_exists(
	int *exists,
	git_refdb *refdb,
	const char *ref_name);

int git_refdb_lookup(
	git_reference **out,
	git_refdb *refdb,
	const char *ref_name);

int git_refdb_rename(
	git_reference **out,
	git_refdb *db,
	const char *old_name,
	const char *new_name,
	int force,
	const git_signature *who,
	const char *message);

int git_refdb_iterator(git_reference_iterator **out, git_refdb *db, const char *glob);
int git_refdb_iterator_next(git_reference **out, git_reference_iterator *iter);
int git_refdb_iterator_next_name(const char **out, git_reference_iterator *iter);
void git_refdb_iterator_free(git_reference_iterator *iter);

int git_refdb_write(git_refdb *refdb, git_reference *ref, int force, const git_signature *who, const char *message, const git_oid *old_id, const char *old_target);
int git_refdb_delete(git_refdb *refdb, const char *ref_name, const git_oid *old_id, const char *old_target);

int git_refdb_reflog_read(git_reflog **out, git_refdb *db,  const char *name);
int git_refdb_reflog_write(git_reflog *reflog);

/**
 * Determine whether a reflog entry should be created for the given reference.
 *
 * Whether or not writing to a reference should create a reflog entry is
 * dependent on a number of things. Most importantly, there's the
 * "core.logAllRefUpdates" setting that controls in which situations a
 * reference should get a corresponding reflog entry. The following values for
 * it are understood:
 *
 *     - "false": Do not log reference updates.
 *
 *     - "true": Log normal reference updates. This will write entries for
 *               references in "refs/heads", "refs/remotes", "refs/notes" and
 *               "HEAD" or if the reference already has a log entry.
 *
 *     - "always": Always create a reflog entry.
 *
 * If unset, the value will default to "true" for non-bare repositories and
 * "false" for bare ones.
 *
 * @param out pointer to which the result will be written, `1` means a reflog
 *            entry should be written, `0` means none should be written.
 * @param db The refdb to decide this for.
 * @param ref The reference one wants to check.
 * @return `0` on success, a negative error code otherwise.
 */
int git_refdb_should_write_reflog(int *out, git_refdb *db, const git_reference *ref);

int git_refdb_has_log(git_refdb *db, const char *refname);
int git_refdb_ensure_log(git_refdb *refdb, const char *refname);

int git_refdb_lock(void **payload, git_refdb *db, const char *refname);
int git_refdb_unlock(git_refdb *db, void *payload, int success, int update_reflog, const git_reference *ref, const git_signature *sig, const char *message);

#endif
