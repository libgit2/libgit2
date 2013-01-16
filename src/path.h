/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_path_h__
#define INCLUDE_path_h__

#include "common.h"
#include "buffer.h"
#include "vector.h"

/**
 * Path manipulation utils
 *
 * These are path utilities that munge paths without actually
 * looking at the real filesystem.
 */

/*
 * The dirname() function shall take a pointer to a character string
 * that contains a pathname, and return a pointer to a string that is a
 * pathname of the parent directory of that file. Trailing '/' characters
 * in the path are not counted as part of the path.
 *
 * If path does not contain a '/', then dirname() shall return a pointer to
 * the string ".". If path is a null pointer or points to an empty string,
 * dirname() shall return a pointer to the string "." .
 *
 * The `git_path_dirname` implementation is thread safe. The returned
 * string must be manually free'd.
 *
 * The `git_path_dirname_r` implementation writes the dirname to a `git_buf`
 * if the buffer pointer is not NULL.
 * It returns an error code < 0 if there is an allocation error, otherwise
 * the length of the dirname (which will be > 0).
 */
extern char *git_path_dirname(const char *path);
extern int git_path_dirname_r(git_buf *buffer, const char *path);

/*
 * This function returns the basename of the file, which is the last
 * part of its full name given by fname, with the drive letter and
 * leading directories stripped off. For example, the basename of
 * c:/foo/bar/file.ext is file.ext, and the basename of a:foo is foo.
 *
 * Trailing slashes and backslashes are significant: the basename of
 * c:/foo/bar/ is an empty string after the rightmost slash.
 *
 * The `git_path_basename` implementation is thread safe. The returned
 * string must be manually free'd.
 *
 * The `git_path_basename_r` implementation writes the basename to a `git_buf`.
 * It returns an error code < 0 if there is an allocation error, otherwise
 * the length of the basename (which will be >= 0).
 */
extern char *git_path_basename(const char *path);
extern int git_path_basename_r(git_buf *buffer, const char *path);

/* Return the offset of the start of the basename.  Unlike the other
 * basename functions, this returns 0 if the path is empty.
 */
extern size_t git_path_basename_offset(git_buf *buffer);

extern const char *git_path_topdir(const char *path);

/**
 * Find offset to root of path if path has one.
 *
 * This will return a number >= 0 which is the offset to the start of the
 * path, if the path is rooted (i.e. "/rooted/path" returns 0 and
 * "c:/windows/rooted/path" returns 2).  If the path is not rooted, this
 * returns < 0.
 */
extern int git_path_root(const char *path);

/**
 * Ensure path has a trailing '/'.
 */
extern int git_path_to_dir(git_buf *path);

/**
 * Ensure string has a trailing '/' if there is space for it.
 */
extern void git_path_string_to_dir(char* path, size_t size);

/**
 * Taken from git.git; returns nonzero if the given path is "." or "..".
 */
GIT_INLINE(int) git_path_is_dot_or_dotdot(const char *name)
{
	return (name[0] == '.' &&
			  (name[1] == '\0' ||
				(name[1] == '.' && name[2] == '\0')));
}

#ifdef GIT_WIN32
GIT_INLINE(int) git_path_is_dot_or_dotdotW(const wchar_t *name)
{
	return (name[0] == L'.' &&
			  (name[1] == L'\0' ||
				(name[1] == L'.' && name[2] == L'\0')));
}

/**
 * Convert backslashes in path to forward slashes.
 */
GIT_INLINE(void) git_path_mkposix(char *path)
{
	while (*path) {
		if (*path == '\\')
			*path = '/';

		path++;
	}
}
#else
#	define git_path_mkposix(p) /* blank */
#endif

extern int git__percent_decode(git_buf *decoded_out, const char *input);

/**
 * Extract path from file:// URL.
 */
extern int git_path_fromurl(git_buf *local_path_out, const char *file_url);


/**
 * Path filesystem utils
 *
 * These are path utilities that actually access the filesystem.
 */

/**
 * Check if a file exists and can be accessed.
 * @return true or false
 */
extern bool git_path_exists(const char *path);

/**
 * Check if the given path points to a directory.
 * @return true or false
 */
extern bool git_path_isdir(const char *path);

/**
 * Check if the given path points to a regular file.
 * @return true or false
 */
extern bool git_path_isfile(const char *path);

/**
 * Check if the given path is a directory, and is empty.
 */
extern bool git_path_is_empty_dir(const char *path);

/**
 * Stat a file and/or link and set error if needed.
 */
extern int git_path_lstat(const char *path, struct stat *st);

/**
 * Check if the parent directory contains the item.
 *
 * @param dir Directory to check.
 * @param item Item that might be in the directory.
 * @return 0 if item exists in directory, <0 otherwise.
 */
extern bool git_path_contains(git_buf *dir, const char *item);

/**
 * Check if the given path contains the given subdirectory.
 *
 * @param parent Directory path that might contain subdir
 * @param subdir Subdirectory name to look for in parent
 * @param append_if_exists If true, then subdir will be appended to the parent path if it does exist
 * @return true if subdirectory exists, false otherwise.
 */
extern bool git_path_contains_dir(git_buf *parent, const char *subdir);

/**
 * Check if the given path contains the given file.
 *
 * @param dir Directory path that might contain file
 * @param file File name to look for in parent
 * @param append_if_exists If true, then file will be appended to the path if it does exist
 * @return true if file exists, false otherwise.
 */
extern bool git_path_contains_file(git_buf *dir, const char *file);

/**
 * Prepend base to unrooted path or just copy path over.
 *
 * This will optionally return the index into the path where the "root"
 * is, either the end of the base directory prefix or the path root.
 */
extern int git_path_join_unrooted(
	git_buf *path_out, const char *path, const char *base, ssize_t *root_at);

/**
 * Clean up path, prepending base if it is not already rooted.
 */
extern int git_path_prettify(git_buf *path_out, const char *path, const char *base);

/**
 * Clean up path, prepending base if it is not already rooted and
 * appending a slash.
 */
extern int git_path_prettify_dir(git_buf *path_out, const char *path, const char *base);

/**
 * Get a directory from a path.
 *
 * If path is a directory, this acts like `git_path_prettify_dir`
 * (cleaning up path and appending a '/').  If path is a normal file,
 * this prettifies it, then removed the filename a la dirname and
 * appends the trailing '/'.  If the path does not exist, it is
 * treated like a regular filename.
 */
extern int git_path_find_dir(git_buf *dir, const char *path, const char *base);

/**
 * Resolve relative references within a path.
 *
 * This eliminates "./" and "../" relative references inside a path,
 * as well as condensing multiple slashes into single ones.  It will
 * not touch the path before the "ceiling" length.
 *
 * Additionally, this will recognize an "c:/" drive prefix or a "xyz://" URL
 * prefix and not touch that part of the path.
 */
extern int git_path_resolve_relative(git_buf *path, size_t ceiling);

/**
 * Apply a relative path to base path.
 *
 * Note that the base path could be a filename or a URL and this
 * should still work.  The relative path is walked segment by segment
 * with three rules: series of slashes will be condensed to a single
 * slash, "." will be eaten with no change, and ".." will remove a
 * segment from the base path.
 */
extern int git_path_apply_relative(git_buf *target, const char *relpath);

/**
 * Walk each directory entry, except '.' and '..', calling fn(state).
 *
 * @param pathbuf buffer the function reads the initial directory
 * 		path from, and updates with each successive entry's name.
 * @param fn function to invoke with each entry. The first arg is
 *		the input state and the second arg is pathbuf. The function
 *		may modify the pathbuf, but only by appending new text.
 * @param state to pass to fn as the first arg.
 * @return 0 on success, GIT_EUSER on non-zero callback, or error code
 */
extern int git_path_direach(
	git_buf *pathbuf,
	int (*fn)(void *, git_buf *),
	void *state);

/**
 * Sort function to order two paths
 */
extern int git_path_cmp(
	const char *name1, size_t len1, int isdir1,
	const char *name2, size_t len2, int isdir2);

/** Path sort function that is case insensitive */
extern int git_path_icmp(
	const char *name1, size_t len1, int isdir1,
	const char *name2, size_t len2, int isdir2);

/**
 * Invoke callback up path directory by directory until the ceiling is
 * reached (inclusive of a final call at the root_path).
 *
 * Returning anything other than 0 from the callback function
 * will stop the iteration and propogate the error to the caller.
 *
 * @param pathbuf Buffer the function reads the directory from and
 *		and updates with each successive name.
 * @param ceiling Prefix of path at which to stop walking up.  If NULL,
 *      this will walk all the way up to the root.  If not a prefix of
 *      pathbuf, the callback will be invoked a single time on the
 *      original input path.
 * @param fn Function to invoke on each path.  The first arg is the
 *		input satte and the second arg is the pathbuf.  The function
 *		should not modify the pathbuf.
 * @param state Passed to fn as the first ath.
 */
extern int git_path_walk_up(
	git_buf *pathbuf,
	const char *ceiling,
	int (*fn)(void *state, git_buf *),
	void *state);

/**
 * Load all directory entries (except '.' and '..') into a vector.
 *
 * For cases where `git_path_direach()` is not appropriate, this
 * allows you to load the filenames in a directory into a vector
 * of strings. That vector can then be sorted, iterated, or whatever.
 * Remember to free alloc of the allocated strings when you are done.
 *
 * @param path The directory to read from.
 * @param prefix_len When inserting entries, the trailing part of path
 * 		will be prefixed after this length.  I.e. given path "/a/b" and
 * 		prefix_len 3, the entries will look like "b/e1", "b/e2", etc.
 * @param alloc_extra Extra bytes to add to each string allocation in
 * 		case you want to append anything funny.
 * @param contents Vector to fill with directory entry names.
 */
extern int git_path_dirload(
	const char *path,
	size_t prefix_len,
	size_t alloc_extra,
	git_vector *contents);


typedef struct {
	struct stat st;
	size_t      path_len;
	char        path[GIT_FLEX_ARRAY];
} git_path_with_stat;

extern int git_path_with_stat_cmp(const void *a, const void *b);
extern int git_path_with_stat_cmp_icase(const void *a, const void *b);

/**
 * Load all directory entries along with stat info into a vector.
 *
 * This adds four things on top of plain `git_path_dirload`:
 *
 * 1. Each entry in the vector is a `git_path_with_stat` struct that
 *    contains both the path and the stat info
 * 2. The entries will be sorted alphabetically
 * 3. Entries that are directories will be suffixed with a '/'
 * 4. Optionally, you can be a start and end prefix and only elements
 *    after the start and before the end (inclusively) will be stat'ed.
 *
 * @param path The directory to read from
 * @param prefix_len The trailing part of path to prefix to entry paths
 * @param ignore_case How to sort and compare paths with start/end limits
 * @param start_stat As optimization, only stat values after this prefix
 * @param end_stat As optimization, only stat values before this prefix
 * @param contents Vector to fill with git_path_with_stat structures
 */
extern int git_path_dirload_with_stat(
	const char *path,
	size_t prefix_len,
	bool ignore_case,
	const char *start_stat,
	const char *end_stat,
	git_vector *contents);

#endif
