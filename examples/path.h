/**
 * Path-processing utilities for the libgit2 examples.
 *
 * Written by the libgit2 contributors
 *
 * To the extent possible under law, the author(s) have dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 *
 * You should have received a copy of the CC0 Public Domain Dedication along
 * with this software. If not, see
 * <http://creativecommons.org/publicdomain/zero/1.0/>.
 */
#ifndef INCLUDE_examples_path_h__
#define INCLUDE_examples_path_h__

/**
 * Returns a new string (caller must free) that is
 * the path to [target] relative to [relative_to].
 *
 * [relative_to] should be a directory.
 */
extern void path_relative_to(char **out_path,
		const char *target_path, const char *relative_to);

/**
 * Get the absolute form of a path with any leading '~'
 * expanded to the user's home directory. *path must not be null.
 *
 * Will free *path before reallocation.
 */
extern void expand_path(char **path);

/**
 * Join two paths with a separator and return a string (via out)
 * that must be freed.
 */
extern void join_paths(char **out, const char *path_a, const char *path_b);

/**
 * Get a pointer to the beginning of the region after the last '.' (and '/') in
 * path. If no '.' is in path, returns a pointer to the end of the path.
 */
extern const char * file_extension_from_path(const char * path);

/**
 * Test the path library's functionality. In debug mode,
 * rather than returning, the program crashes.
 *
 * Returns a non-zero value on failure.
 */
extern int test_path_lib(void);

#endif
