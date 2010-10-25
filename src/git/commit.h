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
 * @param repo the repo to use when locating the commit.
 * @param id identity of the commit to locate.  If the object is
 *        an annotated tag it will be peeled back to the commit.
 * @return the commit; NULL if the commit could not be created
 */
GIT_EXTERN(git_commit *) git_commit_lookup(git_repository *repo, const git_oid *id);

/*
 * Create a new in-memory git_commit.
 *
 * The commit object must be manually filled using
 * setter methods before it can be written to its
 * repository.
 *
 * @param repo The repository where the object will reside
 * @return the object if creation was possible; NULL otherwise
 */
GIT_EXTERN(git_commit *) git_commit_new(git_repository *repo);

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

/*
 * Add a new parent commit to an existing commit
 * @param commit the commit object
 * @param new_parent the new commit which will be a parent
 */
GIT_EXTERN(void) git_commit_add_parent(git_commit *commit, git_commit *new_parent);

/*
 * Set the message of a commit
 * @param commit the commit object
 * @param message the new message
 */
GIT_EXTERN(void) git_commit_set_message(git_commit *commit, const char *message);

/*
 * Set the committer of a commit
 * @param commit the commit object
 * @param committer the new committer
 */
GIT_EXTERN(void) git_commit_set_committer(git_commit *commit, const git_person *committer);

/*
 * Set the author of a commit
 * @param commit the commit object
 * @param author the new author
 */
GIT_EXTERN(void) git_commit_set_author(git_commit *commit, const git_person *author);

/*
 * Set the tree which is pointed to by a commit
 * @param commit the commit object
 * @param tree the new tree
 */
GIT_EXTERN(void) git_commit_set_tree(git_commit *commit, git_tree *tree);

/** @} */
GIT_END_DECL
#endif
