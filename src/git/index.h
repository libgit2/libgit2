#ifndef INCLUDE_git_index_h__
#define INCLUDE_git_index_h__

#include "common.h"
#include "oid.h"

/**
 * @file git/index.h
 * @brief Git index parsing and manipulation routines
 * @defgroup git_index Git index parsing and manipulation routines
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/** Memory representation of an index file. */
typedef struct git_index git_index;

/** Memory representation of a file entry in the index. */
typedef struct git_index_entry git_index_entry;


/**
 * Create a new Git index object as a memory representation
 * of the Git index file in 'index_path'.
 *
 * @param index_path the path to the index file in disk
 * @return the index object; NULL if the index could not be created
 */
GIT_EXTERN(git_index *) git_index_alloc(const char *index_path);

/**
 * Clear the contents (all the entries) of an index object.
 * This clears the index object in memory; changes must be manually
 * written to disk for them to take effect.
 *
 * @param index an existing index object
 */
GIT_EXTERN(void) git_index_clear(git_index *index);

/**
 * Free an existing index object.
 *
 * @param index an existing index object
 */
GIT_EXTERN(void) git_index_free(git_index *index);

/**
 * Update the contents of an existing index object in memory
 * by reading from the hard disk.
 *
 * @param index an existing index object
 * @return 0 on success, otherwise an error code
 */
GIT_EXTERN(int) git_index_read(git_index *index);

/**
 * Write an existing index object from memory back to disk
 * using an atomic file lock.
 *
 * @param index an existing index object
 * @return 0 on success, otherwise an error code
 */
GIT_EXTERN(int) git_index_write(git_index *index);

/**
 * Find the first index of any entires which point to given
 * path in the Git index.
 *
 * @param index an existing index object
 * @param path path to search
 * @return an index >= 0 if found, -1 otherwise
 */
GIT_EXTERN(int) git_index_find(git_index *index, const char *path);

/**
 * Add a new empty entry to the index.
 *
 * @param index an existing index object
 * @param path filename pointed to by the entry
 * @param stage stage for the entry
 * @return 0 on success, otherwise an error code
 */
GIT_EXTERN(int) git_index_add(git_index *index, const char *path, int stage);

/** @} */
GIT_END_DECL
#endif
