/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_note_h__
#define INCLUDE_git_note_h__

#include "oid.h"

/**
 * @file git2/notes.h
 * @brief Git notes management routines
 * @defgroup git_note Git notes management routines
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * Read the note for an object
 *
 * The note must be freed manually by the user.
 *
 * @param note the note; NULL in case of error
 * @param repo the Git repository
 * @param notes_ref OID reference to use (optional); defaults to "refs/notes/commits"
 * @param oid OID of the object
 *
 * @return GIT_SUCCESS or an error code
 */
GIT_EXTERN(int) git_note_read(git_note **note, git_repository *repo,
			      const char *notes_ref, const git_oid *oid);

/**
 * Get the note message
 *
 * @param note
 * @return the note message
 */
GIT_EXTERN(const char *) git_note_message(git_note *note);


/**
 * Get the note object OID
 *
 * @param note
 * @return the note object OID
 */
GIT_EXTERN(const git_oid *) git_note_oid(git_note *note);


/**
 * Add a note for an object
 *
 * @param oid pointer to store the OID (optional); NULL in case of error
 * @param repo the Git repository
 * @param author signature of the notes commit author
 * @param committer signature of the notes commit committer
 * @param notes_ref OID reference to update (optional); defaults to "refs/notes/commits"
 * @param oid The OID of the object
 * @param oid The note to add for object oid
 *
 * @return GIT_SUCCESS or an error code
 */
GIT_EXTERN(int) git_note_create(git_oid *out, git_repository *repo,
				git_signature *author, git_signature *committer,
				const char *notes_ref, const git_oid *oid,
				 const char *note);


/**
 * Remove the note for an object
 *
 * @param repo the Git repository
 * @param notes_ref OID reference to use (optional); defaults to "refs/notes/commits"
 * @param author signature of the notes commit author
 * @param committer signature of the notes commit committer
 * @param oid the oid which note's to be removed
 *
 * @return GIT_SUCCESS or an error code
 */
GIT_EXTERN(int) git_note_remove(git_repository *repo, const char *notes_ref,
				git_signature *author, git_signature *committer,
				const git_oid *oid);

/**
 * Free a git_note object
 *
 * @param note git_note object
 */
GIT_EXTERN(void) git_note_free(git_note *note);

/** @} */
GIT_END_DECL
#endif
