/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_fileops_h__
#define INCLUDE_fileops_h__

#include "common.h"
#include "map.h"
#include "posix.h"
#include "path.h"

/**
 * Filebuffer methods
 *
 * Read whole files into an in-memory buffer for processing
 */
extern int git_futils_readbuffer(git_buf *obj, const char *path);
extern int git_futils_readbuffer_updated(git_buf *obj, const char *path, time_t *mtime, int *updated);

/**
 * File utils
 *
 * These are custom filesystem-related helper methods. They are
 * rather high level, and wrap the underlying POSIX methods
 *
 * All these methods return 0 on success,
 * or an error code on failure and an error message is set.
 */

/**
 * Create and open a file, while also
 * creating all the folders in its path
 */
extern int git_futils_creat_withpath(const char *path, const mode_t dirmode, const mode_t mode);

/**
 * Create an open a process-locked file
 */
extern int git_futils_creat_locked(const char *path, const mode_t mode);

/**
 * Create an open a process-locked file, while
 * also creating all the folders in its path
 */
extern int git_futils_creat_locked_withpath(const char *path, const mode_t dirmode, const mode_t mode);

/**
 * Create a path recursively
 */
extern int git_futils_mkdir_r(const char *path, const char *base, const mode_t mode);

/**
 * Create all the folders required to contain
 * the full path of a file
 */
extern int git_futils_mkpath2file(const char *path, const mode_t mode);

typedef enum {
	GIT_DIRREMOVAL_EMPTY_HIERARCHY = 0,
	GIT_DIRREMOVAL_FILES_AND_DIRS = 1,
	GIT_DIRREMOVAL_ONLY_EMPTY_DIRS = 2,
} git_directory_removal_type;

/**
 * Remove path and any files and directories beneath it.
 *
 * @param path Path to to top level directory to process.
 *
 * @param removal_type GIT_DIRREMOVAL_EMPTY_HIERARCHY to remove a hierarchy
 * of empty directories (will fail if any file is found), GIT_DIRREMOVAL_FILES_AND_DIRS
 * to remove a hierarchy of files and folders, GIT_DIRREMOVAL_ONLY_EMPTY_DIRS to only remove
 * empty directories (no failure on file encounter).
 *
 * @return 0 on success; -1 on error.
 */
extern int git_futils_rmdir_r(const char *path, git_directory_removal_type removal_type);

/**
 * Create and open a temporary file with a `_git2_` suffix.
 * Writes the filename into path_out.
 * @return On success, an open file descriptor, else an error code < 0.
 */
extern int git_futils_mktmp(git_buf *path_out, const char *filename);

/**
 * Move a file on the filesystem, create the
 * destination path if it doesn't exist
 */
extern int git_futils_mv_withpath(const char *from, const char *to, const mode_t dirmode);

/**
 * Open a file readonly and set error if needed.
 */
extern int git_futils_open_ro(const char *path);

/**
 * Get the filesize in bytes of a file
 */
extern git_off_t git_futils_filesize(git_file fd);

#define GIT_MODE_PERMS_MASK			0777
#define GIT_CANONICAL_PERMS(MODE)	(((MODE) & 0100) ? 0755 : 0644)
#define GIT_MODE_TYPE(MODE)			((MODE) & ~GIT_MODE_PERMS_MASK)

/**
 * Convert a mode_t from the OS to a legal git mode_t value.
 */
extern mode_t git_futils_canonical_mode(mode_t raw_mode);


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
 * - 0 on success;
 * - -1 on error.
 */
extern int git_futils_mmap_ro(
	git_map *out,
	git_file fd,
	git_off_t begin,
	size_t len);

/**
 * Read-only map an entire file.
 *
 * @param out buffer to populate with the mapping information.
 * @param path path to file to be opened.
 * @return
 * - 0 on success;
 * - GIT_ENOTFOUND if not found;
 * - -1 on an unspecified OS related error.
 */
extern int git_futils_mmap_ro_file(
	git_map *out,
	const char *path);

/**
 * Release the memory associated with a previous memory mapping.
 * @param map the mapping description previously configured.
 */
extern void git_futils_mmap_free(git_map *map);

/**
 * Find a "global" file (i.e. one in a user's home directory).
 *
 * @param pathbuf buffer to write the full path into
 * @param filename name of file to find in the home directory
 * @return
 * - 0 if found;
 * - GIT_ENOTFOUND if not found;
 * - -1 on an unspecified OS related error.
 */
extern int git_futils_find_global_file(git_buf *path, const char *filename);

/**
 * Find a "system" file (i.e. one shared for all users of the system).
 *
 * @param pathbuf buffer to write the full path into
 * @param filename name of file to find in the home directory
 * @return
 * - 0 if found;
 * - GIT_ENOTFOUND if not found;
 * - -1 on an unspecified OS related error.
 */
extern int git_futils_find_system_file(git_buf *path, const char *filename);

#endif /* INCLUDE_fileops_h__ */
