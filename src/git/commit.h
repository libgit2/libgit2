#ifndef INCLUDE_git_commit_h__
#define INCLUDE_git_commit_h__

#include "common.h"
#include "oid.h"
#include "tree.h"
#include "repository.h"

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
GIT_EXTERN(int) git_commit_lookup(git_commit **commit, git_repository *repo, const git_oid *id);

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
GIT_EXTERN(int) git_commit_new(git_commit ** commit, git_repository *repo);

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
GIT_EXTERN(const git_person *) git_commit_committer(git_commit *commit);

/**
 * Get the author of a commit.
 * @param commit a previously loaded commit.
 * @return the author of a commit
 */
GIT_EXTERN(const git_person *) git_commit_author(git_commit *commit);

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
 */
GIT_EXTERN(void) git_commit_add_parent(git_commit *commit, git_commit *new_parent);

/**
 * Set the message of a commit
 * @param commit the commit object
 * @param message the new message
 */
GIT_EXTERN(void) git_commit_set_message(git_commit *commit, const char *message);

/**
 * Set the committer of a commit
 * @param commit the commit object
 * @param name name of the new committer
 * @param email email of the new committer
 * @param time time when the committer committed the commit
 */
GIT_EXTERN(void) git_commit_set_committer(git_commit *commit, const char *name, const char *email, time_t time);

/**
 * Set the author of a commit
 * @param commit the commit object
 * @param name name of the new author
 * @param email email of the new author
 * @param time time when the author created the commit
 */
GIT_EXTERN(void) git_commit_set_author(git_commit *commit, const char *name, const char *email, time_t time);

/**
 * Set the tree which is pointed to by a commit
 * @param commit the commit object
 * @param tree the new tree
 */
GIT_EXTERN(void) git_commit_set_tree(git_commit *commit, git_tree *tree);

/** @} */
GIT_END_DECL
#endif
