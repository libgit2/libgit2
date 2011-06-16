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
#ifndef INCLUDE_git_tag_h__
#define INCLUDE_git_tag_h__

#include "common.h"
#include "types.h"
#include "oid.h"
#include "object.h"

/**
 * @file git2/tag.h
 * @brief Git tag parsing routines
 * @defgroup git_tag Git tag management
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * Lookup a tag object from the repository.
 *
 * @param tag pointer to the looked up tag
 * @param repo the repo to use when locating the tag.
 * @param id identity of the tag to locate.
 * @return 0 on success; error code otherwise
 */
GIT_INLINE(int) git_tag_lookup(git_tag **tag, git_repository *repo, const git_oid *id)
{
	return git_object_lookup((git_object **)tag, repo, id, (git_otype)GIT_OBJ_TAG);
}

/**
 * Lookup a tag object from the repository,
 * given a prefix of its identifier (short id).
 *
 * @see git_object_lookup_prefix
 *
 * @param tag pointer to the looked up tag
 * @param repo the repo to use when locating the tag.
 * @param id identity of the tag to locate.
 * @param len the length of the short identifier
 * @return 0 on success; error code otherwise
 */
GIT_INLINE(int) git_tag_lookup_prefix(git_tag **tag, git_repository *repo, const git_oid *id, unsigned int len)
{
	return git_object_lookup_prefix((git_object **)tag, repo, id, len, (git_otype)GIT_OBJ_TAG);
}

/**
 * Close an open tag
 *
 * This is a wrapper around git_object_close()
 *
 * IMPORTANT:
 * It *is* necessary to call this method when you stop
 * using a tag. Failure to do so will cause a memory leak.
 *
 * @param tag the tag to close
 */

GIT_INLINE(void) git_tag_close(git_tag *tag)
{
	git_object_close((git_object *) tag);
}


/**
 * Get the id of a tag.
 *
 * @param tag a previously loaded tag.
 * @return object identity for the tag.
 */
GIT_EXTERN(const git_oid *) git_tag_id(git_tag *tag);

/**
 * Get the tagged object of a tag
 *
 * This method performs a repository lookup for the
 * given object and returns it
 *
 * @param target pointer where to store the target
 * @param tag a previously loaded tag.
 * @return 0 on success; error code otherwise
 */
GIT_EXTERN(int) git_tag_target(git_object **target, git_tag *tag);

/**
 * Get the OID of the tagged object of a tag
 *
 * @param tag a previously loaded tag.
 * @return pointer to the OID
 */
GIT_EXTERN(const git_oid *) git_tag_target_oid(git_tag *tag);

/**
 * Get the type of a tag's tagged object
 *
 * @param tag a previously loaded tag.
 * @return type of the tagged object
 */
GIT_EXTERN(git_otype) git_tag_type(git_tag *tag);

/**
 * Get the name of a tag
 *
 * @param tag a previously loaded tag.
 * @return name of the tag
 */
GIT_EXTERN(const char *) git_tag_name(git_tag *tag);

/**
 * Get the tagger (author) of a tag
 *
 * @param tag a previously loaded tag.
 * @return reference to the tag's author
 */
GIT_EXTERN(const git_signature *) git_tag_tagger(git_tag *tag);

/**
 * Get the message of a tag
 *
 * @param tag a previously loaded tag.
 * @return message of the tag
 */
GIT_EXTERN(const char *) git_tag_message(git_tag *tag);


/**
 * Create a new tag in the repository from an OID
 *
 * @param oid Pointer where to store the OID of the
 * newly created tag. If the tag already exists, this parameter
 * will be the oid of the existed tag, and the function will
 * return a GIT_EEXISTS error code.
 *
 * @param repo Repository where to store the tag
 *
 * @param tag_name Name for the tag; this name is validated
 * for consistency. It should also not conflict with an 
 * already existing tag name
 *
 * @param target OID to which this tag points; note that no
 *	validation is done on this OID. Use the _o version of this
 *	method to assure a proper object is being tagged
 *
 * @param target_type Type of the tagged OID; note that no
 *	validation is performed here either
 *
 * @param tagger Signature of the tagger for this tag, and
 *  of the tagging time
 *
 * @param message Full message for this tag
 *
 * @return 0 on success; error code otherwise.
 *	A tag object is written to the ODB, and a proper reference
 *	is written in the /refs/tags folder, pointing to it
 */
GIT_EXTERN(int) git_tag_create(
		git_oid *oid,
		git_repository *repo,
		const char *tag_name,
		const git_oid *target,
		git_otype target_type,
		const git_signature *tagger,
		const char *message);


/**
 * Create a new tag in the repository from an existing
 * `git_object` instance
 *
 * This method replaces the `target` and `target_type`
 * paremeters of `git_tag_create` by a single instance
 * of a `const git_object *`, which is assured to be
 * a proper object in the ODB and hence will create
 * a valid tag
 *
 * @see git_tag_create
 */
GIT_EXTERN(int) git_tag_create_o(
		git_oid *oid,
		git_repository *repo,
		const char *tag_name,
		const git_object *target,
		const git_signature *tagger,
		const char *message);

/**
 * Create a new tag in the repository from a buffer
 *
 * @param oid Pointer where to store the OID of the newly created tag
 *
 * @param repo Repository where to store the tag
 *
 * @param buffer Raw tag data
 */
GIT_EXTERN(int) git_tag_create_frombuffer(
		git_oid *oid,
		git_repository *repo,
		const char *buffer);

/**
 * Create a new tag in the repository from an OID
 * and overwrite an already existing tag reference, if any.
 *
 * @param oid Pointer where to store the OID of the
 *	newly created tag
 *
 * @param repo Repository where to store the tag
 *
 * @param tag_name Name for the tag; this name is validated
 * for consistency.
 *
 * @param target OID to which this tag points; note that no
 *	validation is done on this OID. Use the _fo version of this
 *	method to assure a proper object is being tagged
 *
 * @param target_type Type of the tagged OID; note that no
 *	validation is performed here either
 *
 * @param tagger Signature of the tagger for this tag, and
 *  of the tagging time
 *
 * @param message Full message for this tag
 *
 * @return 0 on success; error code otherwise.
 *	A tag object is written to the ODB, and a proper reference
 *	is written in the /refs/tags folder, pointing to it
 */
GIT_EXTERN(int) git_tag_create_f(
		git_oid *oid,
		git_repository *repo,
		const char *tag_name,
		const git_oid *target,
		git_otype target_type,
		const git_signature *tagger,
		const char *message);

/**
 * Create a new tag in the repository from an existing
 * `git_object` instance and overwrite an already existing 
 * tag reference, if any.
 *
 * This method replaces the `target` and `target_type`
 * paremeters of `git_tag_create_f` by a single instance
 * of a `const git_object *`, which is assured to be
 * a proper object in the ODB and hence will create
 * a valid tag
 *
 * @see git_tag_create_f
 */
GIT_EXTERN(int) git_tag_create_fo(
		git_oid *oid,
		git_repository *repo,
		const char *tag_name,
		const git_object *target,
		const git_signature *tagger,
		const char *message);

/**
 * Delete an existing tag reference.
 *
 * @param repo Repository where lives the tag
 *
 * @param tag_name Name of the tag to be deleted;
 * this name is validated for consistency.
 *
 * @return 0 on success; error code otherwise.
 */
GIT_EXTERN(int) git_tag_delete(
		git_repository *repo,
		const char *tag_name);

/**
 * Fill a list with all the tags in the Repository
 *
 * The string array will be filled with the names of the
 * matching tags; these values are owned by the user and
 * should be free'd manually when no longer needed, using
 * `git_strarray_free`.
 *
 * @param tag_names Pointer to a git_strarray structure where
 *		the tag names will be stored
 * @param repo Repository where to find the tags
 * @return 0 on success; error code otherwise
 */
GIT_EXTERN(int) git_tag_list(
		git_strarray *tag_names,
		git_repository *repo);

/** @} */
GIT_END_DECL
#endif
