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
#include "repository.h"

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
 * The generated tag object is owned by the revision
 * repo and shall not be freed by the user.
 *
 * @param tag pointer to the looked up tag
 * @param repo the repo to use when locating the tag.
 * @param id identity of the tag to locate.
 * @return 0 on success; error code otherwise
 */
GIT_INLINE(int) git_tag_lookup(git_tag **tag, git_repository *repo, const git_oid *id)
{
	return git_repository_lookup((git_object **)tag, repo, id, GIT_OBJ_TAG);
}

/**
 * Create a new in-memory git_tag.
 *
 * The tag object must be manually filled using
 * setter methods before it can be written to its
 * repository.
 *
 * @param tag pointer to the new tag
 * @param repo The repository where the object will reside
 * @return 0 on success; error code otherwise
 */
GIT_INLINE(int) git_tag_new(git_tag **tag, git_repository *repo)
{
	return git_repository_newobject((git_object **)tag, repo, GIT_OBJ_TAG);
}

/**
 * Get the id of a tag.
 * @param tag a previously loaded tag.
 * @return object identity for the tag.
 */
GIT_EXTERN(const git_oid *) git_tag_id(git_tag *tag);

/**
 * Get the tagged object of a tag
 * @param tag a previously loaded tag.
 * @return reference to a repository object
 */
GIT_EXTERN(const git_object *) git_tag_target(git_tag *t);

/**
 * Get the type of a tag's tagged object
 * @param tag a previously loaded tag.
 * @return type of the tagged object
 */
GIT_EXTERN(git_otype) git_tag_type(git_tag *t);

/**
 * Get the name of a tag
 * @param tag a previously loaded tag.
 * @return name of the tag
 */
GIT_EXTERN(const char *) git_tag_name(git_tag *t);

/**
 * Get the tagger (author) of a tag
 * @param tag a previously loaded tag.
 * @return reference to the tag's author
 */
GIT_EXTERN(const git_signature *) git_tag_tagger(git_tag *t);

/**
 * Get the message of a tag
 * @param tag a previously loaded tag.
 * @return message of the tag
 */
GIT_EXTERN(const char *) git_tag_message(git_tag *t);

/**
 * Set the target of a tag (i.e. the object that the tag points to)
 * @param tag The tag to modify
 * @param target the new tagged target
 */
GIT_EXTERN(void) git_tag_set_target(git_tag *tag, git_object *target);

/**
 * Set the name of a tag
 * @param tag The tag to modify
 * @param name the new name for the tag
 */
GIT_EXTERN(void) git_tag_set_name(git_tag *tag, const char *name);

/**
 * Set the tagger of a tag
 * @param tag The tag to modify
 * @param tagger_sig signature of the tagging action
 */
GIT_EXTERN(void) git_tag_set_tagger(git_tag *tag, const git_signature *tagger_sig);

/**
 * Set the message of a tag
 * @param tag The tag to modify
 * @param message the new tagger for the tag
 */
GIT_EXTERN(void) git_tag_set_message(git_tag *tag, const char *message);

/** @} */
GIT_END_DECL
#endif
