#ifndef INCLUDE_git_commit_h__
#define INCLUDE_git_commit_h__

#include "common.h"
#include "oid.h"
#include "tree.h"

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

/** Parsed representation of an author/committer of a commit */
typedef struct git_commit_person {
	char name[64]; /**< Full name */
	char email[64]; /**< Email address */
	time_t time; /**< Time when this person commited the change */
} git_commit_person;

/**
 * Locate a reference to a commit without loading it.
 * The generated commit object is owned by the revision
 * pool and shall not be freed by the user.
 *
 * @param pool the pool to use when locating the commit.
 * @param id identity of the commit to locate.  If the object is
 *        an annotated tag it will be peeled back to the commit.
 * @return the commit; NULL if the commit could not be created
 */
GIT_EXTERN(git_commit *) git_commit_lookup(git_revpool *pool, const git_oid *id);

/**
 * Locate a reference to a commit, and try to load and parse it it from
 * the commit cache or the object database.
 * The generated commit object is owned by the revision
 * pool and shall not be freed by the user.
 *
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
 * Get the committer of a commit.
 * @param commit a previously loaded commit.
 * @return the committer of a commit
 */
GIT_EXTERN(const git_commit_person *) git_commit_committer(git_commit *commit);

/**
 * Get the author of a commit.
 * @param commit a previously loaded commit.
 * @return the author of a commit
 */
GIT_EXTERN(const git_commit_person *) git_commit_author(git_commit *commit);

/**
 * Get the tree pointed to by a commit.
 * @param commit a previously loaded commit.
 * @return the tree of a commit
 */
GIT_EXTERN(const git_tree *) git_commit_tree(git_commit *commit);

/** @} */
GIT_END_DECL
#endif
