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
GIT_EXTERN(git_object *) git_repository_lookup(git_repository *repo, const git_oid *id, git_otype type);

/**
 * Get the object database behind a Git repository
 *
 * @param repo a repository object
 * @return a pointer to the object db
 */
GIT_EXTERN(git_odb *) git_repository_database(git_repository *repo);

/**
 * Create a new in-memory repository object with
 * the given type.
 *
 * The object's attributes can be filled in using the
 * corresponding setter methods.
 *
 * The object will be written back to given git_repository
 * when the git_object_write() function is called; objects
 * cannot be written to disk until all their main
 * attributes have been properly filled.
 *
 * Objects are instantiated with no SHA1 id; their id
 * will be automatically generated when writing to the
 * repository.
 *
 * @parem repo Repository where the object belongs
 * @param type Type of the object to be created
 * @return the new object
 */
GIT_EXTERN(git_object *) git_object_new(git_repository *repo, git_otype type);

/**
 * Write back an object to disk.
 *
 * The object will be written to its corresponding
 * repository.
 *
 * If the object has no changes since it was first
 * read from the repository, no actions will take place.
 *
 * If the object has been modified since it was read from
 * the repository, or it has been created from scratch
 * in memory, it will be written to the repository and
 * its SHA1 ID will be updated accordingly.
 *
 * @param object Git object to write back
 * @return 0 on success; otherwise an error code
 */
GIT_EXTERN(int) git_object_write(git_object *object);

/**
 * Get the id (SHA1) of a repository object
 *
 * In-memory objects created by git_object_new() do not
 * have a SHA1 ID until they are written on a repository.
 *
 * @param obj the repository object
 * @return the SHA1 id
 */
GIT_EXTERN(const git_oid *) git_object_id(git_object *obj);

/**
 * Get the object type of an object
 *
 * @param obj the repository object
 * @return the object's type
 */
GIT_EXTERN(git_otype) git_object_type(git_object *obj);

/**
 * Get the repository that owns this object
 *
 * @param obj the object
 * @return the repository who owns this object
 */
GIT_EXTERN(git_repository *) git_object_owner(git_object *obj);

/**
 * Free a reference to one of the objects in the repository.
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
GIT_EXTERN(void) git_object_free(git_object *object);

/**
 * Free a previously allocated repository
 * @param repo repository handle to close. If NULL nothing occurs.
 */
GIT_EXTERN(void) git_repository_free(git_repository *repo);

/** @} */
GIT_END_DECL
#endif
