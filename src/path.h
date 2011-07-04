/*
 * posix.h - Path management methods
 */
#ifndef INCLUDE_path_h__
#define INCLUDE_path_h__

#include "common.h"

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
 * The `git_path_dirname_r` implementation expects a string allocated
 * by the user with big enough size.
 */
extern char *git_path_dirname(const char *path);
extern int git_path_dirname_r(char *buffer, size_t bufflen, const char *path);

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
 * The `git_path_basename_r` implementation expects a string allocated
 * by the user with big enough size.
 */
extern char *git_path_basename(const char *path);
extern int git_path_basename_r(char *buffer, size_t bufflen, const char *path);

extern const char *git_path_topdir(const char *path);

/**
 * Join two paths together. Takes care of properly fixing the
 * middle slashes and everything
 *
 * The paths are joined together into buffer_out; this is expected
 * to be an user allocated buffer of `GIT_PATH_MAX` size
 */
extern void git_path_join_n(char *buffer_out, int npath, ...);

GIT_INLINE(void) git_path_join(char *buffer_out, const char *path_a, const char *path_b)
{
	git_path_join_n(buffer_out, 2, path_a, path_b);
}

int git_path_root(const char *path);

int git_path_prettify(char *path_out, const char *path, const char *base);
int git_path_prettify_dir(char *path_out, const char *path, const char *base);

#ifdef GIT_WIN32
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

#endif
