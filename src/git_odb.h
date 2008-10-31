/*
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following
 *   disclaimer in the documentation and/or other materials provided
 *   with the distribution.
 *
 * - Neither the name of the Git Development Community nor the
 *   names of its contributors may be used to endorse or promote
 *   products derived from this software without specific prior
 *   written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef INCLUDE_git_odb_h__
#define INCLUDE_git_odb_h__

#include "git_common.h"
#include "git_oid.h"
#include <unistd.h>

/**
 * @file git_odb.h
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
GIT_EXTERN(git_result) git_odb_open(git_odb **out, const char *objects_dir);

/**
 * Close an open object database.
 * @param db database pointer to close.  If NULL no action is taken.
 *           The pointer is set to NULL when the close is completed.
 */
GIT_EXTERN(void) git_odb_close(git_odb** db);

/** Basic type (loose or packed) of any Git object. */
typedef enum {
	OBJ_BAD = -1,       /**< Object is invalid. */
	OBJ__EXT1 = 0,      /**< Reserved for future use. */
	OBJ_COMMIT = 1,     /**< A commit object. */
	OBJ_TREE = 2,       /**< A tree (directory listing) object. */
	OBJ_BLOB = 3,       /**< A file revision object. */
	OBJ_TAG = 4,        /**< An annotated tag object. */
	OBJ__EXT2 = 5,      /**< Reserved for future use. */
	OBJ_OFS_DELTA = 6,  /**< A delta, base is given by an offset. */
	OBJ_REF_DELTA = 7,  /**< A delta, base is given by object id. */
} git_otype;

/** A small object read from the database. */
typedef struct {
	void *data;      /**< Raw, decompressed object data. */
	size_t len;      /**< Total number of bytes in data. */
	git_otype type;  /**< Type of this object. */
} git_sobj;

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
GIT_EXTERN(git_result) git_odb_read(git_sobj *out, git_odb *db, const git_oid *id);

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
GIT_EXTERN(git_result) git_odb__read_packed(git_sobj *out, git_odb *db, const git_oid *id);

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
GIT_EXTERN(git_result) git_odb__read_loose(git_sobj *out, git_odb *db, const git_oid *id);

/**
 * Release all memory used by the sobj structure.
 *
 * As a result of this call, obj->data will be set to NULL.
 *
 * If obj->data is already NULL, nothing happens.
 *
 * @param obj object descriptor to free.
 */
GIT_EXTERN(void) git_sobj_close(git_sobj *obj);

/** @} */
GIT_END_DECL
#endif
