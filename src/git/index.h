#ifndef INCLUDE_git_index_h__
#define INCLUDE_git_index_h__

#include <stdint.h>
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


/** Time used in a git index entry */
typedef struct {
	uint32_t seconds;
	uint32_t nanoseconds;
} git_index_time;

/** Memory representation of a file entry in the index. */
typedef struct git_index_entry {
	git_index_time ctime;
	git_index_time mtime;

	uint32_t dev;
	uint32_t ino;
	uint32_t mode;
	uint32_t uid;
	uint32_t gid;
	uint32_t file_size;

	git_oid oid;

	uint16_t flags;
	uint16_t flags_extended;

	char *path;
} git_index_entry;


/**
 * Create a new Git index object as a memory representation
 * of the Git index file in 'index_path'.
 *
 * The argument 'working_dir' is the root path of the indexed
 * files in the index and is used to calculate the relative path
 * when inserting new entries from existing files on disk.
 *
 * If 'working _dir' is NULL (e.g for bare repositories), the
 * methods working on on-disk files will fail.
 *
 * @param index the pointer for the new index
 * @param index_path the path to the index file in disk
 * @param working_dir working dir for the git repository
 * @return 0 on success; error code otherwise
 */
GIT_EXTERN(int) git_index_open(git_index **index, const char *index_path, const char *working_dir);

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
 * Add a new empty entry to the index with a given path.
 *
 * @param index an existing index object
 * @param path filename pointed to by the entry
 * @param stage stage for the entry
 * @return 0 on success, otherwise an error code
 */
GIT_EXTERN(int) git_index_add_bypath(git_index *index, const char *path, int stage);

/**
 * Remove an entry from the index 
 *
 * @param index an existing index object
 * @param position position of the entry to remove
 * @return 0 on success, otherwise an error code
 */
GIT_EXTERN(int) git_index_remove(git_index *index, int position);

/**
 * Add a new entry to the index 
 *
 * @param index an existing index object
 * @param source_entry new entry object
 * @return 0 on success, otherwise an error code
 */
GIT_EXTERN(int) git_index_add(git_index *index, const git_index_entry *source_entry);

/**
 * Get a pointer to one of the entries in the index
 *
 * This entry can be modified, and the changes will be written
 * back to disk on the next write() call.
 *
 * @param index an existing index object
 * @param n the position of the entry
 * @return a pointer to the entry; NULL if out of bounds
 */
GIT_EXTERN(git_index_entry *) git_index_get(git_index *index, int n);


/** @} */
GIT_END_DECL
#endif
