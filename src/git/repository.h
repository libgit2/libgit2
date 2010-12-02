#ifndef INCLUDE_git_repository_h__
#define INCLUDE_git_repository_h__

#include "common.h"
#include "odb.h"
#include "commit.h"
#include "index.h"

/**
 * @file git/repository.h
 * @brief Git revision object management routines
 * @defgroup git_repository Git revision object management routines
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * Open a git repository.
 *
 * The 'path' argument must point to an existing git repository
 * folder, e.g.
 *
 *		/path/to/my_repo/.git/	(normal repository)
 *							objects/
 *							index
 *							HEAD
 *
 *		/path/to/bare_repo/		(bare repository)
 *						objects/
 *						index
 *						HEAD
 *
 *	The method will automatically detect if 'path' is a normal
 *	or bare repository or fail is 'path' is neither.
 *
 * @param repository pointer to the repo which will be opened
 * @param path the path to the repository
 * @return 0 on sucess; error code otherwise
 */
GIT_EXTERN(int) git_repository_open(git_repository **repository, const char *path);


/**
 * Open a git repository by manually specifying all its paths
 *
 * @param repository pointer to the repo which will be opened
 *
 * @param git_dir The full path to the repository folder
 *		e.g. a '.git' folder for live repos, any folder for bare
 *		Equivalent to $GIT_DIR. 
 *		Cannot be NULL.
 *
 * @param git_object_directory The full path to the ODB folder.
 *		the folder where all the loose and packed objects are stored
 *		Equivalent to $GIT_OBJECT_DIRECTORY.
 *		If NULL, "$GIT_DIR/objects/" is assumed.
 *
 * @param git_index_file The full path to the index (dircache) file
 *		Equivalent to $GIT_INDEX_FILE.
 *		If NULL, "$GIT_DIR/index" is assumed.
 *
 * @param git_work_tree The full path to the working tree of the repository,
 *		if the repository is not bare.
 *		Equivalent to $GIT_WORK_TREE.
 *		If NULL, the repository is assumed to be bare.
 *
 * @return 0 on sucess; error code otherwise
 */
GIT_EXTERN(int) git_repository_open2(git_repository **repository,
		const char *git_dir,
		const char *git_object_directory,
		const char *git_index_file,
		const char *git_work_tree);


/**
 * Lookup a reference to one of the objects in the repostory.
 *
 * The generated reference is owned by the repository and
 * should not be freed by the user.
 *
 * The 'type' parameter must match the type of the object
 * in the odb; the method will fail otherwise.
 * The special value 'GIT_OBJ_ANY' may be passed to let
 * the method guess the object's type.
 *
 * @param object pointer to the looked-up object
 * @param repo the repository to look up the object
 * @param id the unique identifier for the object
 * @param type the type of the object
 * @return a reference to the object
 */
GIT_EXTERN(int) git_repository_lookup(git_object **object, git_repository *repo, const git_oid *id, git_otype type);

/**
 * Get the object database behind a Git repository
 *
 * @param repo a repository object
 * @return a pointer to the object db
 */
GIT_EXTERN(git_odb *) git_repository_database(git_repository *repo);

/**
 * Get the Index file of a Git repository
 *
 * @param repo a repository object
 * @return a pointer to the Index object; 
 *	NULL if the index cannot be opened
 */
GIT_EXTERN(git_index *) git_repository_index(git_repository *rpeo);

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
 * @param object pointer to the new object
 * @parem repo Repository where the object belongs
 * @param type Type of the object to be created
 * @return the new object
 */
GIT_EXTERN(int) git_repository_newobject(git_object **object, git_repository *repo, git_otype type);

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
