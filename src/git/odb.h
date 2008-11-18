/*
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2,
 * as published by the Free Software Foundation.
 *
 * In addition to the permissions in the GNU General Public License,
 * the authors give you unlimited permission to link the compiled
 * version of this file into combinations with other programs,
 * and to distribute those combinations without any restriction
 * coming from the use of this file.  (The General Public License
 * restrictions do apply in other respects; for example, they cover
 * modification of the file, and distribution when not linked into
 * a combined executable.)
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef INCLUDE_git_odb_h__
#define INCLUDE_git_odb_h__

#include "common.h"
#include "oid.h"

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

/** A small object read from the database. */
typedef struct {
	void *data;          /**< Raw, decompressed object data. */
	size_t len;          /**< Total number of bytes in data. */
	git_otype type;      /**< Type of this object. */
} git_obj;

/**
 * Read a small object from the database.
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
 * Read a small object from the database using only pack files.
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
 * Read a small object from the database using only loose object files.
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
 * Release all memory used by the sobj structure.
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

/** @} */
GIT_END_DECL
#endif
