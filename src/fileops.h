/*
 * fileops.h - OS agnostic disk io operations
 *
 * This header describes the strictly internal part of the api
 */
#ifndef INCLUDE_fileops_h__
#define INCLUDE_fileops_h__

#include "common.h"
#include "map.h"
#include "dir.h"
#include "posix.h"
#include "path.h"

/**
 * Filebuffer methods
 *
 * Read whole files into an in-memory buffer for processing
 */
#define GIT_FBUFFER_INIT {NULL, 0}

typedef struct {  /* file io buffer  */
	void *data;  /* data bytes   */
	size_t len;  /* data length  */
} git_fbuffer;

extern int git_futils_readbuffer(git_fbuffer *obj, const char *path);
extern void git_futils_freebuffer(git_fbuffer *obj);

/**
 * File utils
 *
 * These are custom filesystem-related helper methods. They are
 * rather high level, and wrap the underlying POSIX methods
 *
 * All these methods return GIT_SUCCESS on success,
 * or an error code on failure and an error message is set.
 */

/**
 * Check if a file exists and can be accessed.
 */
extern int git_futils_exists(const char *path);

/**
 * Create and open a file, while also
 * creating all the folders in its path
 */
extern int git_futils_creat_withpath(const char *path, int mode);

/**
 * Create an open a process-locked file
 */
extern int git_futils_creat_locked(const char *path, int mode);

/**
 * Create an open a process-locked file, while
 * also creating all the folders in its path
 */
extern int git_futils_creat_locked_withpath(const char *path, int mode);

/**
 * Check if the given path points to a directory
 */
extern int git_futils_isdir(const char *path);

/**
 * Check if the given path points to a regular file
 */
extern int git_futils_isfile(const char *path);

/**
 * Create a path recursively
 */
extern int git_futils_mkdir_r(const char *path, int mode);

/**
 * Create all the folders required to contain
 * the full path of a file
 */
extern int git_futils_mkpath2file(const char *path);

/**
 * Create and open a temporary file with a `_git2_` suffix
 */
extern int git_futils_mktmp(char *path_out, const char *filename);

/**
 * Atomically rename a file on the filesystem
 */
extern int git_futils_mv_atomic(const char *from, const char *to);

/**
 * Move a file on the filesystem, create the
 * destination path if it doesn't exist
 */
extern int git_futils_mv_withpath(const char *from, const char *to);


/**
 * Get the filesize in bytes of a file
 */
extern git_off_t git_futils_filesize(git_file fd);

/**
 * Read-only map all or part of a file into memory.
 * When possible this function should favor a virtual memory
 * style mapping over some form of malloc()+read(), as the
 * data access will be random and is not likely to touch the
 * majority of the region requested.
 *
 * @param out buffer to populate with the mapping information.
 * @param fd open descriptor to configure the mapping from.
 * @param begin first byte to map, this should be page aligned.
 * @param end number of bytes to map.
 * @return
 * - GIT_SUCCESS on success;
 * - GIT_EOSERR on an unspecified OS related error.
 */
extern int git_futils_mmap_ro(
	git_map *out,
	git_file fd,
	git_off_t begin,
	size_t len);

/**
 * Release the memory associated with a previous memory mapping.
 * @param map the mapping description previously configured.
 */
extern void git_futils_mmap_free(git_map *map);

/**
 * Walk each directory entry, except '.' and '..', calling fn(state).
 *
 * @param pathbuf buffer the function reads the initial directory
 * 		path from, and updates with each successive entry's name.
 * @param pathmax maximum allocation of pathbuf.
 * @param fn function to invoke with each entry.  The first arg is
 *		the input state and the second arg is pathbuf.  The function
 *		may modify the pathbuf, but only by appending new text.
 * @param state to pass to fn as the first arg.
 */
extern int git_futils_direach(
	char *pathbuf,
	size_t pathmax,
	int (*fn)(void *, char *),
	void *state);

extern int git_futils_cmp_path(const char *name1, int len1, int isdir1,
		const char *name2, int len2, int isdir2);


/**
 * Clean up a provided absolute or relative directory path.
 *
 * This prettification relies on basic operations such as coalescing
 * multiple forward slashes into a single slash, removing '.' and
 * './' current directory segments, and removing parent directory
 * whenever '..' is encountered.
 *
 * If not empty, the returned path ends with a forward slash.
 *
 * For instance, this will turn "d1/s1///s2/..//../s3" into "d1/s3/".
 *
 * This only performs a string based analysis of the path.
 * No checks are done to make sure the path actually makes sense from
 * the file system perspective.
 *
 * @param buffer_out buffer to populate with the normalized path.
 * @param size buffer size.
 * @param path directory path to clean up.
 * @return
 * - GIT_SUCCESS on success;
 * - GIT_ERROR when the input path is invalid or escapes the current directory.
 */
int git_futils_prettify_dir(char *buffer_out, size_t size, const char *path, const char *base_path);

/**
 * Clean up a provided absolute or relative file path.
 *
 * This prettification relies on basic operations such as coalescing
 * multiple forward slashes into a single slash, removing '.' and
 * './' current directory segments, and removing parent directory
 * whenever '..' is encountered.
 *
 * For instance, this will turn "d1/s1///s2/..//../s3" into "d1/s3".
 *
 * This only performs a string based analysis of the path.
 * No checks are done to make sure the path actually makes sense from
 * the file system perspective.
 *
 * @param buffer_out buffer to populate with the normalized path.
 * @param size buffer size.
 * @param path file path to clean up.
 * @return
 * - GIT_SUCCESS on success;
 * - GIT_ERROR when the input path is invalid or escapes the current directory.
 */
int git_futils_prettyify_file(char *buffer_out, size_t size, const char *path, const char *base_path);

int git_futils_root_offset(const char *path);

#endif /* INCLUDE_fileops_h__ */
