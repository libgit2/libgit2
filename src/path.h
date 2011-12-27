/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_path_h__
#define INCLUDE_path_h__

#include "common.h"
#include "buffer.h"

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

extern const char *git_path_topdir(const char *path);

extern int git_path_root(const char *path);

extern int git_path_prettify(git_buf *path_out, const char *path, const char *base);
extern int git_path_prettify_dir(git_buf *path_out, const char *path, const char *base);

extern int git_path_to_dir(git_buf *path);
extern void git_path_string_to_dir(char* path, size_t size);

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

extern int git__percent_decode(git_buf *decoded_out, const char *input);
extern int git_path_fromurl(git_buf *local_path_out, const char *file_url);

#endif
