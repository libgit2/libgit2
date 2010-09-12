#ifndef INCLUDE_git_repository_h__
#define INCLUDE_git_repository_h__

#include "common.h"
#include "odb.h"
#include "commit.h"

/**
 * @file git/repository.h
 * @brief Git revision object management routines
 * @defgroup git_repository Git revision object management routines
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * Allocate a new repository object.
 *
 * TODO: specify the repository's path instead
 * of its object database
 *
 * @param odb an existing object database to back the repo
 * @return the new repository handle; NULL on error 
 */
GIT_EXTERN(git_repository *) git_repository_alloc(git_odb *odb);


/**
 * Lookup a reference to one of the objects in the repostory.
 * 
 * The generated reference is owned by the repository and
 * should not be freed by the user.
 * The generated reference should be cast back to the
 * expected type; e.g.
 *
 *	git_commit *c = (git_commit *)
 *		git_repository_lookup(repo, id, GIT_OBJ_COMMIT);
 *
 * The 'type' parameter must match the type of the object
 * in the odb; the method will fail otherwise.
 * The special value 'GIT_OBJ_ANY' may be passed to let
 * the method guess the object's type.
 *
 * @param repo the repository to look up the object
 * @param id the unique identifier for the object
 * @param type the type of the object
 * @return a reference to the object
 */
GIT_EXTERN(git_repository_object *) git_repository_lookup(git_repository *repo, const git_oid *id, git_otype type);

/**
 * Get the object database behind a Git repository
 *
 * @param repo a repository object
 * @return a pointer to the object db
 */
GIT_EXTERN(git_odb *) git_repository_database(git_repository *repo);

/**
 * Get the id (SHA1) of a repository object
 *
 * @param obj the repository object
 * @return the SHA1 id
 */
const git_oid *git_repository_object_id(git_repository_object *obj);

/**
 * Get the object type of an object
 *
 * @param obj the repository object
 * @return the object's type
 */
git_otype git_repository_object_type(git_repository_object *obj);

/**
 * Free a reference to one of the objects in the repostory.
 *
 * Repository objects are managed automatically by the library,
 * but this method can be used to force freeing one of the
 * objects.
 *
 * Careful: freeing objects in the middle of a repository
 * traversal will most likely cause errors.
 *
 * @param object the object to free
 */
GIT_EXTERN(void) git_repository_object_free(git_repository_object *object);

/**
 * Free a previously allocated repository
 * @param repo repository handle to close. If NULL nothing occurs.
 */
GIT_EXTERN(void) git_repository_free(git_repository *repo);

/** @} */
GIT_END_DECL
#endif
