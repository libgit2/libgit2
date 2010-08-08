#ifndef INCLUDE_git_tag_h__
#define INCLUDE_git_tag_h__

#include "common.h"
#include "oid.h"
#include "tree.h"
#include "repository.h"

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
 * Lookup a tag object from the repository.
 * The generated tag object is owned by the revision
 * repo and shall not be freed by the user.
 *
 * @param repo the repo to use when locating the tag.
 * @param id identity of the tag to locate.
 * @return the tag; NULL if the tag could not be created
 */
GIT_EXTERN(git_tag *) git_tag_lookup(git_repository *repo, const git_oid *id);

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
