#ifndef INCLUDE_git_odb_h__
#define INCLUDE_git_odb_h__

#include "common.h"
#include "oid.h"
#include <stdlib.h>

/**
 * @file git/odb.h
 * @brief Git object database routines
 * @defgroup git_odb Git object database routines
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/** An open object database handle. */
typedef struct git_odb git_odb;

/** A custom backend in an ODB */
typedef struct git_odb_backend git_odb_backend;

/**
 * Create a new object database with no backends.
 *
 * Before the ODB can be used for read/writing, a custom database
 * backend must be manually added using `git_odb_add_backend()`
 *
 * @param out location to store the database pointer, if opened.
 *            Set to NULL if the open failed.
 * @return GIT_SUCCESS if the database was created; otherwise an error
 *         code describing why the open was not possible.
 */
GIT_EXTERN(int) git_odb_new(git_odb **out);

/**
 * Create a new object database and automatically add
 * the two default backends:
 *
 *	- git_odb_backend_loose: read and write loose object files
 *		from disk, assuming `objects_dir` as the Objects folder
 *
 *	- git_odb_backend_pack: read objects from packfiles,
 *		assuming `objects_dir` as the Objects folder which
 *		contains a 'pack/' folder with the corresponding data
 *
 * @param out location to store the database pointer, if opened.
 *            Set to NULL if the open failed.
 * @param objects_dir path of the backends' "objects" directory.
 * @return GIT_SUCCESS if the database opened; otherwise an error
 *         code describing why the open was not possible.
 */
GIT_EXTERN(int) git_odb_open(git_odb **out, const char *objects_dir);

/**
 * Add a custom backend to an existing Object DB
 *
 * Read <odb_backends.h> for more information.
 *
 * @param odb database to add the backend to
 * @paramm backend pointer to a git_odb_backend instance
 * @return 0 on sucess; error code otherwise
 */
GIT_EXTERN(int) git_odb_add_backend(git_odb *odb, git_odb_backend *backend);

/**
 * Close an open object database.
 * @param db database pointer to close.  If NULL no action is taken.
 */
GIT_EXTERN(void) git_odb_close(git_odb *db);

/** Basic type (loose or packed) of any Git object. */
typedef enum {
	GIT_OBJ_ANY = -2,		/**< Object can be any of the following */
	GIT_OBJ_BAD = -1,       /**< Object is invalid. */
	GIT_OBJ__EXT1 = 0,      /**< Reserved for future use. */
	GIT_OBJ_COMMIT = 1,     /**< A commit object. */
	GIT_OBJ_TREE = 2,       /**< A tree (directory listing) object. */
	GIT_OBJ_BLOB = 3,       /**< A file revision object. */
	GIT_OBJ_TAG = 4,        /**< An annotated tag object. */
	GIT_OBJ__EXT2 = 5,      /**< Reserved for future use. */
	GIT_OBJ_OFS_DELTA = 6,  /**< A delta, base is given by an offset. */
	GIT_OBJ_REF_DELTA = 7,  /**< A delta, base is given by object id. */
} git_otype;

/** An object read from the database. */
typedef struct {
	void *data;          /**< Raw, decompressed object data. */
	size_t len;          /**< Total number of bytes in data. */
	git_otype type;      /**< Type of this object. */
} git_rawobj;

/**
 * Read an object from the database.
 *
 * If GIT_ENOTFOUND then out->data is set to NULL.
 *
 * @param out object descriptor to populate upon reading.
 * @param db database to search for the object in.
 * @param id identity of the object to read.
 * @return
 * - GIT_SUCCESS if the object was read;
 * - GIT_ENOTFOUND if the object is not in the database.
 */
GIT_EXTERN(int) git_odb_read(git_rawobj *out, git_odb *db, const git_oid *id);

/**
 * Read the header of an object from the database, without
 * reading its full contents.
 *
 * Only the 'type' and 'len' fields of the git_rawobj structure
 * are filled. The 'data' pointer will always be NULL.
 *
 * The raw object pointed by 'out' doesn't need to be manually
 * closed with git_rawobj_close().
 *
 * @param out object descriptor to populate upon reading.
 * @param db database to search for the object in.
 * @param id identity of the object to read.
 * @return
 * - GIT_SUCCESS if the object was read;
 * - GIT_ENOTFOUND if the object is not in the database.
 */
GIT_EXTERN(int) git_odb_read_header(git_rawobj *out, git_odb *db, const git_oid *id);

/**
 * Write an object to the database.
 *
 * @param id identity of the object written.
 * @param db database to which the object should be written.
 * @param obj object descriptor for the object to write.
 * @return
 * - GIT_SUCCESS if the object was written;
 * - GIT_ERROR otherwise.
 */
GIT_EXTERN(int) git_odb_write(git_oid *id, git_odb *db, git_rawobj *obj);

/**
 * Determine if the given object can be found in the object database.
 *
 * @param db database to be searched for the given object.
 * @param id the object to search for.
 * @return
 * - true, if the object was found
 * - false, otherwise
 */
GIT_EXTERN(int) git_odb_exists(git_odb *db, const git_oid *id);





/**
 * Determine the object-ID (sha1 hash) of the given git_rawobj.
 *
 * The input obj must be a valid loose object type and the data
 * pointer must not be NULL, unless the len field is also zero.
 *
 * @param id the resulting object-ID.
 * @param obj the object whose hash is to be determined.
 * @return
 * - GIT_SUCCESS if the object-ID was correctly determined.
 * - GIT_ERROR if the given object is malformed.
 */
GIT_EXTERN(int) git_rawobj_hash(git_oid *id, git_rawobj *obj);

/**
 * Release all memory used by the obj structure.
 *
 * As a result of this call, obj->data will be set to NULL.
 *
 * If obj->data is already NULL, nothing happens.
 *
 * @param obj object descriptor to free.
 */
GIT_INLINE(void) git_rawobj_close(git_rawobj *obj)
{
	free(obj->data);
	obj->data = NULL;
}




/**
 * Convert an object type to it's string representation.
 *
 * The result is a pointer to a string in static memory and
 * should not be free()'ed.
 *
 * @param type object type to convert.
 * @return the corresponding string representation.
 */
GIT_EXTERN(const char *) git_otype_tostring(git_otype type);

/**
 * Convert a string object type representation to it's git_otype.
 *
 * @param str the string to convert.
 * @return the corresponding git_otype.
 */
GIT_EXTERN(git_otype) git_otype_fromstring(const char *str);

/**
 * Determine if the given git_otype is a valid loose object type.
 *
 * @param type object type to test.
 * @return true if the type represents a valid loose object type,
 * false otherwise.
 */
GIT_EXTERN(int) git_otype_is_loose(git_otype type);

/** @} */
GIT_END_DECL
#endif
