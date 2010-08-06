#ifndef INCLUDE_git_tag_h__
#define INCLUDE_git_tag_h__

#include "common.h"
#include "oid.h"
#include "tree.h"

/**
 * @file git/tag.h
 * @brief Git tag parsing routines
 * @defgroup git_tag Git tag management
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/** Parsed representation of a tag object. */
typedef struct git_tag git_tag;

/**
 * Locate a reference to a tag without loading it.
 * The generated tag object is owned by the revision
 * pool and shall not be freed by the user.
 *
 * @param pool the pool to use when locating the tag.
 * @param id identity of the tag to locate.
 * @return the tag; NULL if the tag could not be created
 */
GIT_EXTERN(git_tag *) git_tag_lookup(git_revpool *pool, const git_oid *id);

/**
 * Locate a reference to a tag, and try to load and parse it it from
 * the object cache or the object database.
 * The generated tag object is owned by the revision
 * pool and shall not be freed by the user.
 *
 * @param pool the pool to use when parsing/caching the tag.
 * @param id identity of the tag to locate.  
 * @return the tag; NULL if the tag does not exist in the
 *         pool's git_odb, or if the tag is present but is
 *         too malformed to be parsed successfully.
 */
GIT_EXTERN(git_tag *) git_tag_parse(git_revpool *pool, const git_oid *id);

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
GIT_EXTERN(const git_repository_object *) git_tag_target(git_tag *t);

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
GIT_EXTERN(const git_person *) git_tag_tagger(git_tag *t);

/**
 * Get the message of a tag
 * @param tag a previously loaded tag.
 * @return message of the tag
 */
GIT_EXTERN(const char *) git_tag_message(git_tag *t);

/** @} */
GIT_END_DECL
#endif
