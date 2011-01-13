/*
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2,
 * as published by the Free Software Foundation.
 *
 * In addition to the permissions in the GNU General Public License,
 * the authors give you unlimited permission to link the compiled
 * version of this file into combinations with other programs,
 * and to distribute those combinations without any restriction
 * coming from the use of this file.  (The General Public License
 * restrictions do apply in other respects; for example, they cover
 * modification of the file, and distribution when not linked into
 * a combined executable.)
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
#ifndef INCLUDE_git_commit_h__
#define INCLUDE_git_commit_h__

#include "common.h"
#include "types.h"
#include "oid.h"
#include "repository.h"

/**
 * @file git2/commit.h
 * @brief Git commit parsing, formatting routines
 * @defgroup git_commit Git commit parsing, formatting routines
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * Lookup a commit object from a repository.
 * The generated commit object is owned by the revision
 * repo and shall not be freed by the user.
 *
 * @param commit pointer to the looked up commit
 * @param repo the repo to use when locating the commit.
 * @param id identity of the commit to locate.  If the object is
 *        an annotated tag it will be peeled back to the commit.
 * @return 0 on success; error code otherwise
 */
GIT_INLINE(int) git_commit_lookup(git_commit **commit, git_repository *repo, const git_oid *id)
{
	return git_repository_lookup((git_object **)commit, repo, id, GIT_OBJ_COMMIT);
}

/**
 * Create a new in-memory git_commit.
 *
 * The commit object must be manually filled using
 * setter methods before it can be written to its
 * repository.
 *
 * @param commit pointer to the new commit
 * @param repo The repository where the object will reside
 * @return 0 on success; error code otherwise
 */
GIT_INLINE(int) git_commit_new(git_commit **commit, git_repository *repo)
{
	return git_repository_newobject((git_object **)commit, repo, GIT_OBJ_COMMIT);
}

/**
 * Get the id of a commit.
 * @param commit a previously loaded commit.
 * @return object identity for the commit.
 */
GIT_EXTERN(const git_oid *) git_commit_id(git_commit *commit);

/**
 * Get the short (one line) message of a commit.
 * @param commit a previously loaded commit.
 * @return the short message of a commit
 */
GIT_EXTERN(const char *) git_commit_message_short(git_commit *commit);

/**
 * Get the full message of a commit.
 * @param commit a previously loaded commit.
 * @return the message of a commit
 */
GIT_EXTERN(const char *) git_commit_message(git_commit *commit);

/**
 * Get the commit time (i.e. committer time) of a commit.
 * @param commit a previously loaded commit.
 * @return the time of a commit
 */
GIT_EXTERN(time_t) git_commit_time(git_commit *commit);

/**
 * Get the commit timezone offset (i.e. committer's preferred timezone) of a commit.
 * @param commit a previously loaded commit.
 * @return positive or negative timezone offset, in minutes from UTC
 */
GIT_EXTERN(int) git_commit_time_offset(git_commit *commit);

/**
 * Get the committer of a commit.
 * @param commit a previously loaded commit.
 * @return the committer of a commit
 */
GIT_EXTERN(const git_signature *) git_commit_committer(git_commit *commit);

/**
 * Get the author of a commit.
 * @param commit a previously loaded commit.
 * @return the author of a commit
 */
GIT_EXTERN(const git_signature *) git_commit_author(git_commit *commit);

/**
 * Get the tree pointed to by a commit.
 * @param commit a previously loaded commit.
 * @return the tree of a commit
 */
GIT_EXTERN(const git_tree *) git_commit_tree(git_commit *commit);

/**
 * Get the number of parents of this commit
 *
 * @param commit a previously loaded commit.
 * @return integer of count of parents
 */
GIT_EXTERN(unsigned int) git_commit_parentcount(git_commit *commit);

/**
 * Get the specified parent of the commit.
 * @param commit a previously loaded commit.
 * @param n the position of the entry
 * @return a pointer to the commit; NULL if out of bounds
 */
GIT_EXTERN(git_commit *) git_commit_parent(git_commit *commit, unsigned int n);

/**
 * Add a new parent commit to an existing commit
 * @param commit the commit object
 * @param new_parent the new commit which will be a parent
 * @return 0 on success; error code otherwise
 */
GIT_EXTERN(int) git_commit_add_parent(git_commit *commit, git_commit *new_parent);

/**
 * Set the message of a commit
 * @param commit the commit object
 * @param message the new message
 */
GIT_EXTERN(void) git_commit_set_message(git_commit *commit, const char *message);

/**
 * Set the committer of a commit
 * @param commit the commit object
 * @param author_sig signature of the committer
 */
GIT_EXTERN(void) git_commit_set_committer(git_commit *commit, const git_signature *committer_sig);

/**
 * Set the author of a commit
 * @param commit the commit object
 * @param author_sig signature of the author
 */
GIT_EXTERN(void) git_commit_set_author(git_commit *commit, const git_signature *author_sig);

/**
 * Set the tree which is pointed to by a commit
 * @param commit the commit object
 * @param tree the new tree
 */
GIT_EXTERN(void) git_commit_set_tree(git_commit *commit, git_tree *tree);

/** @} */
GIT_END_DECL
#endif
