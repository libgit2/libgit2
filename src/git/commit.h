#ifndef INCLUDE_git_commit_h__
#define INCLUDE_git_commit_h__

#include "common.h"
#include "oid.h"

/**
 * @file git/commit.h
 * @brief Git commit parsing, formatting routines
 * @defgroup git_commit Git commit parsing, formatting routines
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/** Parsed representation of a commit object. */
typedef struct git_commit git_commit;

/**
 * Locate a reference to a commit without loading it.
 * @param pool the pool to use when locating the commit.
 * @param id identity of the commit to locate.  If the object is
 *        an annotated tag it will be peeled back to the commit.
 * @return the commit; NULL if the commit could not be created
 */
GIT_EXTERN(git_commit *) git_commit_lookup(git_revpool *pool, const git_oid *id);

/**
 * Locate a reference to a commit, and try to load and parse it it from
 * the commit cache or the object database.
 * @param pool the pool to use when parsing/caching the commit.
 * @param id identity of the commit to locate.  If the object is
 *        an annotated tag it will be peeled back to the commit.
 * @return the commit; NULL if the commit does not exist in the
 *         pool's git_odb, or if the commit is present but is
 *         too malformed to be parsed successfully.
 */
GIT_EXTERN(git_commit *) git_commit_parse(git_revpool *pool, const git_oid *id);

/**
 * Get the id of a commit.
 * @param commit a previously parsed commit.
 * @return object identity for the commit.
 */
GIT_EXTERN(const git_oid *) git_commit_id(git_commit *commit);

/** @} */
GIT_END_DECL
#endif
