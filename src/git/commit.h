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
 * Parse (or lookup) a commit from a revision pool.
 * @param pool the pool to use when parsing/caching the commit.
 * @param id identity of the commit to locate.  If the object is
 *        an annotated tag it will be peeled back to the commit.
 * @return the commit; NULL if the commit does not exist in the
 *         pool's git_odb, or if the commit is present but is
 *         too malformed to be parsed successfully.
 */
GIT_EXTERN(git_commit*) git_commit_parse(git_revpool *pool, const git_oid *id);

/**
 * Get the id of a commit.
 * @param commit a previously parsed commit.
 * @return object identity for the commit.
 */
GIT_EXTERN(const git_oid*) git_commit_id(git_commit *commit);

/** @} */
GIT_END_DECL
#endif
