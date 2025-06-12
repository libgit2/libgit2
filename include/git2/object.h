/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_object_h__
#define INCLUDE_git_object_h__

#include "common.h"
#include "types.h"
#include "oid.h"
#include "buffer.h"
#include "filter.h"

/**
 * @file git2/object.h
 * @brief Objects are blobs (files), trees (directories), commits, and annotated tags
 * @defgroup git_object Git revision object management routines
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/** Maximum size of a git object */
#define GIT_OBJECT_SIZE_MAX UINT64_MAX

/**
 * Lookup a reference to one of the objects in a repository.
 *
 * The generated reference is owned by the repository and
 * should be closed with the `git_object_free` method
 * instead of free'd manually.
 *
 * The 'type' parameter must match the type of the object
 * in the odb; the method will fail otherwise.
 * The special value 'GIT_OBJECT_ANY' may be passed to let
 * the method guess the object's type.
 *
 * @param object pointer to the looked-up object
 * @param repo the repository to look up the object
 * @param id the unique identifier for the object
 * @param type the type of the object
 * @return 0 or an error code
 */
GIT_EXTERN(int) git_object_lookup(
		git_object **object,
		git_repository *repo,
		const git_oid *id,
		git_object_t type);

/**
 * Lookup a reference to one of the objects in a repository,
 * given a prefix of its identifier (short id).
 *
 * The object obtained will be so that its identifier
 * matches the first 'len' hexadecimal characters
 * (packets of 4 bits) of the given `id`. `len` must be
 * at least `GIT_OID_MINPREFIXLEN`, and long enough to
 * identify a unique object matching the prefix; otherwise
 * the method will fail.
 *
 * The generated reference is owned by the repository and
 * should be closed with the `git_object_free` method
 * instead of free'd manually.
 *
 * The `type` parameter must match the type of the object
 * in the odb; the method will fail otherwise.
 * The special value `GIT_OBJECT_ANY` may be passed to let
 * the method guess the object's type.
 *
 * @param object_out pointer where to store the looked-up object
 * @param repo the repository to look up the object
 * @param id a short identifier for the object
 * @param len the length of the short identifier
 * @param type the type of the object
 * @return 0 or an error code
 */
GIT_EXTERN(int) git_object_lookup_prefix(
		git_object **object_out,
		git_repository *repo,
		const git_oid *id,
		size_t len,
		git_object_t type);


/**
 * Lookup an object that represents a tree entry.
 *
 * @param out buffer that receives a pointer to the object (which must be freed
 *            by the caller)
 * @param treeish root object that can be peeled to a tree
 * @param path relative path from the root object to the desired object
 * @param type type of object desired
 * @return 0 on success, or an error code
 */
GIT_EXTERN(int) git_object_lookup_bypath(
		git_object **out,
		const git_object *treeish,
		const char *path,
		git_object_t type);

/**
 * Get the id (SHA1) of a repository object
 *
 * @param obj the repository object
 * @return the SHA1 id
 */
GIT_EXTERN(const git_oid *) git_object_id(const git_object *obj);

/**
 * Get a short abbreviated OID string for the object
 *
 * This starts at the "core.abbrev" length (default 7 characters) and
 * iteratively extends to a longer string if that length is ambiguous.
 * The result will be unambiguous (at least until new objects are added to
 * the repository).
 *
 * @param out Buffer to write string into
 * @param obj The object to get an ID for
 * @return 0 on success, <0 for error
 */
GIT_EXTERN(int) git_object_short_id(git_buf *out, const git_object *obj);

/**
 * Get the object type of an object
 *
 * @param obj the repository object
 * @return the object's type
 */
GIT_EXTERN(git_object_t) git_object_type(const git_object *obj);

/**
 * Get the repository that owns this object
 *
 * Freeing or calling `git_repository_close` on the
 * returned pointer will invalidate the actual object.
 *
 * Any other operation may be run on the repository without
 * affecting the object.
 *
 * @param obj the object
 * @return the repository who owns this object
 */
GIT_EXTERN(git_repository *) git_object_owner(const git_object *obj);

/**
 * Close an open object
 *
 * This method instructs the library to close an existing
 * object; note that git_objects are owned and cached by the repository
 * so the object may or may not be freed after this library call,
 * depending on how aggressive is the caching mechanism used
 * by the repository.
 *
 * IMPORTANT:
 * It *is* necessary to call this method when you stop using
 * an object. Failure to do so will cause a memory leak.
 *
 * @param object the object to close
 */
GIT_EXTERN(void) git_object_free(git_object *object);

/**
 * Convert an object type to its string representation.
 *
 * The result is a pointer to a string in static memory and
 * should not be free()'ed.
 *
 * @param type object type to convert.
 * @return the corresponding string representation.
 */
GIT_EXTERN(const char *) git_object_type2string(git_object_t type);

/**
 * Convert a string object type representation to it's git_object_t.
 *
 * @param str the string to convert.
 * @return the corresponding git_object_t.
 */
GIT_EXTERN(git_object_t) git_object_string2type(const char *str);

/**
 * Determine if the given git_object_t is a valid object type.
 *
 * @param type object type to test.
 * @return 1 if the type represents a valid loose object type, 0 otherwise
 */
GIT_EXTERN(int) git_object_type_is_valid(git_object_t type);

/**
 * Recursively peel an object until an object of the specified type is met.
 *
 * If the query cannot be satisfied due to the object model,
 * GIT_EINVALIDSPEC will be returned (e.g. trying to peel a blob to a
 * tree).
 *
 * If you pass `GIT_OBJECT_ANY` as the target type, then the object will
 * be peeled until the type changes. A tag will be peeled until the
 * referenced object is no longer a tag, and a commit will be peeled
 * to a tree. Any other object type will return GIT_EINVALIDSPEC.
 *
 * If peeling a tag we discover an object which cannot be peeled to
 * the target type due to the object model, GIT_EPEEL will be
 * returned.
 *
 * You must free the returned object.
 *
 * @param peeled Pointer to the peeled git_object
 * @param object The object to be processed
 * @param target_type The type of the requested object (a GIT_OBJECT_ value)
 * @return 0 on success, GIT_EINVALIDSPEC, GIT_EPEEL, or an error code
 */
GIT_EXTERN(int) git_object_peel(
	git_object **peeled,
	const git_object *object,
	git_object_t target_type);

/**
 * Create an in-memory copy of a Git object. The copy must be
 * explicitly free'd or it will leak.
 *
 * @param dest Pointer to store the copy of the object
 * @param source Original object to copy
 * @return 0 or an error code
 */
GIT_EXTERN(int) git_object_dup(git_object **dest, git_object *source);

/**
 * Options for calculating object IDs from raw content.
 *
 * Initialize with `GIT_OBJECT_ID_OPTIONS_INIT`. Alternatively, you can
 * use `git_object_id_options_init`.
 */
typedef struct {
	unsigned int version; /**< version for the struct */

	/**
	 * Object type of the raw content; if not specified, this
	 * defaults to `GIT_OBJECT_BLOB`.
	 */
	git_object_t object_type;

	/**
	 * Object ID type to generate; if not specified, this defaults
	 * to `GIT_OID_DEFAULT`.
	 */
	git_oid_t oid_type;

	/**
	 * Filters to mutate the raw data with; these are ignored
	 * unless the given raw object data is a blob.
	 */
	git_filter_list *filters;
} git_object_id_options;

/** Current version for the `git_object_id_options` structure */
#define GIT_OBJECT_ID_OPTIONS_VERSION 1

/** Static constructor for `object_id_options` */
#define GIT_OBJECT_ID_OPTIONS_INIT {GIT_OBJECT_ID_OPTIONS_VERSION}

/**
 * Initialize `git_object_id_options` structure with default values.
 * Equivalent to creating an instance with `GIT_WORKTREE_ADD_OPTIONS_INIT`.
 *
 * @param opts The `git_object_id_options` struct to initialize.
 * @param version The struct version; pass `GIT_OBJECT_ID_OPTIONS_INIT`.
 * @return 0 on success; -1 on failure.
 */
GIT_EXTERN(int) git_object_id_options_init(git_object_id_options *opts,
	unsigned int version);

/**
 * Given the raw content of an object, determine the object ID.
 * This prepends the object header to the given data, and hashes
 * the results with the hash corresponding to the given oid_type.
 *
 * @param[out] oid_out the resulting object id
 * @param buf the raw object content
 * @param len the length of the given buffer
 * @param opts the options for id calculation
 * @return 0 on success, or an error code
 */
GIT_EXTERN(int) git_object_id_from_buffer(
	git_oid *oid_out,
	const void *buf,
	size_t len,
	const git_object_id_options *opts);

/**
 * Given an on-disk file that contains the raw content of an object,
 * determine the object ID. This prepends the object header to the given
 * data, and hashes the results with the hash corresponding to the given
 * oid_type.
 *
 * Note that this does not look at attributes or do any on-disk filtering
 * (like line ending translation), so when used with blobs, it may not
 * match the results for adding to the repository. To compute the object
 * ID for a blob with filters, use `git_repository_hashfile`.
 *
 * @see git_repository_hashfile
 *
 * @param[out] oid_out the resulting object id
 * @param path the on-disk path to the raw object content
 * @param opts the options for id calculation
 * @return 0 on success, or an error code
 */
GIT_EXTERN(int) git_object_id_from_file(
	git_oid *oid_out,
	const char *path,
	const git_object_id_options *opts);

#ifdef GIT_EXPERIMENTAL_SHA256
/**
 * Analyzes a buffer of raw object content and determines its validity.
 * Tree, commit, and tag objects will be parsed and ensured that they
 * are valid, parseable content.  (Blobs are always valid by definition.)
 * An error message will be set with an informative message if the object
 * is not valid.
 *
 * @warning This function is experimental and its signature may change in
 * the future.
 *
 * @param valid Output pointer to set with validity of the object content
 * @param buf The contents to validate
 * @param len The length of the buffer
 * @param object_type The type of the object in the buffer
 * @param oid_type The object ID type for the OIDs in the given buffer
 * @return 0 on success or an error code
 */
GIT_EXTERN(int) git_object_rawcontent_is_valid(
	int *valid,
	const char *buf,
	size_t len,
	git_object_t object_type,
	git_oid_t oid_type);
#else
/**
 * Analyzes a buffer of raw object content and determines its validity.
 * Tree, commit, and tag objects will be parsed and ensured that they
 * are valid, parseable content.  (Blobs are always valid by definition.)
 * An error message will be set with an informative message if the object
 * is not valid.
 *
 * @warning This function is experimental and its signature may change in
 * the future.
 *
 * @param[out] valid Output pointer to set with validity of the object content
 * @param buf The contents to validate
 * @param len The length of the buffer
 * @param object_type The type of the object in the buffer
 * @return 0 on success or an error code
 */
GIT_EXTERN(int) git_object_rawcontent_is_valid(
	int *valid,
	const char *buf,
	size_t len,
	git_object_t object_type);
#endif

/** @} */
GIT_END_DECL

#endif
