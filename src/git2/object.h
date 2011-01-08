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
#ifndef INCLUDE_git_object_h__
#define INCLUDE_git_object_h__

#include "common.h"
#include "types.h"
#include "oid.h"

/**
 * @file git2/object.h
 * @brief Git revision object management routines
 * @defgroup git_object Git revision object management routines
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

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
 * Convert an object type to it's string representation.
 *
 * The result is a pointer to a string in static memory and
 * should not be free()'ed.
 *
 * @param type object type to convert.
 * @return the corresponding string representation.
 */
GIT_EXTERN(const char *) git_object_type2string(git_otype type);

/**
 * Convert a string object type representation to it's git_otype.
 *
 * @param str the string to convert.
 * @return the corresponding git_otype.
 */
GIT_EXTERN(git_otype) git_object_string2type(const char *str);

/**
 * Determine if the given git_otype is a valid loose object type.
 *
 * @param type object type to test.
 * @return true if the type represents a valid loose object type,
 * false otherwise.
 */
GIT_EXTERN(int) git_object_typeisloose(git_otype type);

/**
 * Get the size in bytes for the structure which
 * acts as an in-memory representation of any given
 * object type.
 *
 * For all the core types, this would the equivalent
 * of calling `sizeof(git_commit)` if the core types
 * were not opaque on the external API.
 *
 * @param type object type to get its size
 * @return size in bytes of the object
 */
GIT_EXTERN(size_t) git_object__size(git_otype type);

/** @} */
GIT_END_DECL

#endif
