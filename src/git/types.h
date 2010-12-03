#ifndef INCLUDE_git_types_h__
#define INCLUDE_git_types_h__

/**
 * @file git/types.h
 * @brief libgit2 base types
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

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

/** An open object database handle. */
typedef struct git_odb git_odb;

/** A custom backend in an ODB */
typedef struct git_odb_backend git_odb_backend;

/**
 * Representation of an existing git repository,
 * including all its object contents
 */
typedef struct git_repository git_repository;

/** Representation of a generic object in a repository */
typedef struct git_object git_object;

typedef struct git_revwalk git_revwalk;

/** Parsed representation of a tag object. */
typedef struct git_tag git_tag;

/** In-memory representation of a blob object. */
typedef struct git_blob git_blob;

/** Parsed representation of a commit object. */
typedef struct git_commit git_commit;

/** Representation of each one of the entries in a tree object. */
typedef struct git_tree_entry git_tree_entry;

/** Representation of a tree object. */
typedef struct git_tree git_tree;

/** Memory representation of an index file. */
typedef struct git_index git_index;

/** @} */
GIT_END_DECL

#endif
