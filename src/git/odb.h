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

/**
 * Open an object database for read/write access.
 * @param out location to store the database pointer, if opened.
 *            Set to NULL if the open failed.
 * @param objects_dir path of the database's "objects" directory.
 * @return GIT_SUCCESS if the database opened; otherwise an error
 *         code describing why the open was not possible.
 */
GIT_EXTERN(int) git_odb_open(git_odb **out, const char *objects_dir);

/**
 * Close an open object database.
 * @param db database pointer to close.  If NULL no action is taken.
 */
GIT_EXTERN(void) git_odb_close(git_odb *db);

/** Basic type (loose or packed) of any Git object. */
typedef enum {
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
} git_obj;

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
GIT_EXTERN(int) git_odb_read(git_obj *out, git_odb *db, const git_oid *id);

/**
 * Read an object from the database using only pack files.
 *
 * If GIT_ENOTFOUND then out->data is set to NULL.
 *
 * @param out object descriptor to populate upon reading.
 * @param db database to search for the object in.
 * @param id identity of the object to read.
 * @return
 * - GIT_SUCCESS if the object was read.
 * - GIT_ENOTFOUND if the object is not in the database.
 */
GIT_EXTERN(int) git_odb__read_packed(git_obj *out, git_odb *db, const git_oid *id);

/**
 * Read an object from the database using only loose object files.
 *
 * If GIT_ENOTFOUND then out->data is set to NULL.
 *
 * @param out object descriptor to populate upon reading.
 * @param db database to search for the object in.
 * @param id identity of the object to read.
 * @return
 * - GIT_SUCCESS if the object was read.
 * - GIT_ENOTFOUND if the object is not in the database.
 */
GIT_EXTERN(int) git_odb__read_loose(git_obj *out, git_odb *db, const git_oid *id);

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
GIT_EXTERN(int) git_odb_write(git_oid *id, git_odb *db, git_obj *obj);

/**
 * Release all memory used by the obj structure.
 *
 * As a result of this call, obj->data will be set to NULL.
 *
 * If obj->data is already NULL, nothing happens.
 *
 * @param obj object descriptor to free.
 */
GIT_INLINE(void) git_obj_close(git_obj *obj)
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
GIT_EXTERN(const char *) git_obj_type_to_string(git_otype type);

/**
 * Convert a string object type representation to it's git_otype.
 *
 * @param str the string to convert.
 * @return the corresponding git_otype.
 */
GIT_EXTERN(git_otype) git_obj_string_to_type(const char *str);

/**
 * Determine if the given git_otype is a valid loose object type.
 *
 * @param type object type to test.
 * @return true if the type represents a valid loose object type,
 * false otherwise.
 */
GIT_EXTERN(int) git_obj__loose_object_type(git_otype type);

/**
 * Determine the object-ID (sha1 hash) of the given git_obj.
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
GIT_EXTERN(int) git_obj_hash(git_oid *id, git_obj *obj);

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

/** @} */
GIT_END_DECL
#endif
