/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#include "common.h"
#include "path.h"
#include "posix.h"

#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>

/*
 * Based on the Android implementation, BSD licensed.
 * Check http://android.git.kernel.org/
 */
int git_path_basename_r(git_buf *buffer, const char *path)
{
	const char *endp, *startp;
	int len, result;

	/* Empty or NULL string gets treated as "." */
	if (path == NULL || *path == '\0') {
		startp = ".";
		len		= 1;
		goto Exit;
	}

	/* Strip trailing slashes */
	endp = path + strlen(path) - 1;
	while (endp > path && *endp == '/')
		endp--;

	/* All slashes becomes "/" */
	if (endp == path && *endp == '/') {
		startp = "/";
		len	= 1;
		goto Exit;
	}

	/* Find the start of the base */
	startp = endp;
	while (startp > path && *(startp - 1) != '/')
		startp--;

	len = endp - startp +1;

Exit:
	result = len;

	if (buffer != NULL) {
		if (git_buf_set(buffer, startp, len) < GIT_SUCCESS)
			return git__rethrow(git_buf_lasterror(buffer),
				"Could not get basename of '%s'", path);
	}

	return result;
}

/*
 * Based on the Android implementation, BSD licensed.
 * Check http://android.git.kernel.org/
 */
int git_path_dirname_r(git_buf *buffer, const char *path)
{
	const char *endp;
	int result, len;

	/* Empty or NULL string gets treated as "." */
	if (path == NULL || *path == '\0') {
		path = ".";
		len = 1;
		goto Exit;
	}

	/* Strip trailing slashes */
	endp = path + strlen(path) - 1;
	while (endp > path && *endp == '/')
		endp--;

	/* Find the start of the dir */
	while (endp > path && *endp != '/')
		endp--;

	/* Either the dir is "/" or there are no slashes */
	if (endp == path) {
		path = (*endp == '/') ? "/" : ".";
		len = 1;
		goto Exit;
	}

	do {
		endp--;
	} while (endp > path && *endp == '/');

	len = endp - path +1;

#ifdef GIT_WIN32
	/* Mimic unix behavior where '/.git' returns '/': 'C:/.git' will return
		'C:/' here */

	if (len == 2 && isalpha(path[0]) && path[1] == ':') {
		len = 3;
		goto Exit;
	}
#endif

Exit:
	result = len;

	if (buffer != NULL) {
		if (git_buf_set(buffer, path, len) < GIT_SUCCESS)
			return git__rethrow(git_buf_lasterror(buffer),
				"Could not get dirname of '%s'", path);
	}

	return result;
}


char *git_path_dirname(const char *path)
{
	git_buf buf = GIT_BUF_INIT;
	char *dirname;

	git_path_dirname_r(&buf, path);
	dirname = git_buf_detach(&buf);
	git_buf_free(&buf); /* avoid memleak if error occurs */

	return dirname;
}

char *git_path_basename(const char *path)
{
	git_buf buf = GIT_BUF_INIT;
	char *basename;

	git_path_basename_r(&buf, path);
	basename = git_buf_detach(&buf);
	git_buf_free(&buf); /* avoid memleak if error occurs */

	return basename;
}


const char *git_path_topdir(const char *path)
{
	size_t len;
	int i;

	assert(path);
	len = strlen(path);

	if (!len || path[len - 1] != '/')
		return NULL;

	for (i = len - 2; i >= 0; --i)
		if (path[i] == '/')
			break;

	return &path[i + 1];
}

int git_path_root(const char *path)
{
	int offset = 0;

#ifdef GIT_WIN32
	/* Does the root of the path look like a windows drive ? */
	if (isalpha(path[0]) && (path[1] == ':'))
		offset += 2;
#endif

	if (*(path + offset) == '/')
		return offset;

	return -1;	/* Not a real error. Rather a signal than the path is not rooted */
}

int git_path_prettify(git_buf *path_out, const char *path, const char *base)
{
	int  error = GIT_SUCCESS;
	char buf[GIT_PATH_MAX];

	git_buf_clear(path_out);

	/* construct path if needed */
	if (base != NULL && git_path_root(path) < 0) {
		if ((error = git_buf_joinpath(path_out, base, path)) < GIT_SUCCESS)
			return error;
		path = path_out->ptr;
	}

	if (path == NULL || p_realpath(path, buf) == NULL)
		error = GIT_EOSERR;
	else
		error = git_buf_sets(path_out, buf);

	return error;
}

int git_path_prettify_dir(git_buf *path_out, const char *path, const char *base)
{
	int error = git_path_prettify(path_out, path, base);

	if (error == GIT_SUCCESS)
		error = git_path_to_dir(path_out);

	return error;
}

int git_path_to_dir(git_buf *path)
{
	if (path->asize > 0 &&
		path->size > 0 &&
		path->ptr[path->size - 1] != '/')
		git_buf_putc(path, '/');

	return git_buf_lasterror(path);
}

void git_path_string_to_dir(char* path, size_t size)
{
	size_t end = strlen(path);

	if (end && path[end - 1] != '/' && end < size) {
		path[end] = '/';
		path[end + 1] = '\0';
	}
}

int git__percent_decode(git_buf *decoded_out, const char *input)
{
	int len, hi, lo, i, error = GIT_SUCCESS;
	assert(decoded_out && input);

	len = strlen(input);
	git_buf_clear(decoded_out);

	for(i = 0; i < len; i++)
	{
		char c = input[i];

		if (c != '%')
			goto append;

		if (i >= len - 2)
			goto append;

		hi = git__fromhex(input[i + 1]);
		lo = git__fromhex(input[i + 2]);

		if (hi < 0 || lo < 0)
			goto append;

		c = (char)(hi << 4 | lo);
		i += 2;

append:
		error = git_buf_putc(decoded_out, c);
		if (error < GIT_SUCCESS)
			return git__rethrow(error, "Failed to percent decode '%s'.", input);
	}

	return error;
}

int git_path_fromurl(git_buf *local_path_out, const char *file_url)
{
	int error = GIT_SUCCESS, offset = 0, len;

	assert(local_path_out && file_url);

	if (git__prefixcmp(file_url, "file://") != 0)
		return git__throw(GIT_EINVALIDPATH,
			"Parsing of '%s' failed. A file Uri is expected (ie. with 'file://' scheme).",
			file_url);

	offset += 7;
	len = strlen(file_url);

	if (offset < len && file_url[offset] == '/')
		offset++;
	else if (offset < len && git__prefixcmp(file_url + offset, "localhost/") == 0)
		offset += 10;
	else
		return git__throw(GIT_EINVALIDPATH,
			"Parsing of '%s' failed. A local file Uri is expected.", file_url);

	if (offset >= len || file_url[offset] == '/')
		return git__throw(GIT_EINVALIDPATH, 
			"Parsing of '%s' failed. Invalid file Uri format.", file_url);

#ifndef _MSC_VER
	offset--;	/* A *nix absolute path starts with a forward slash */
#endif

	git_buf_clear(local_path_out);

	error = git__percent_decode(local_path_out, file_url + offset);
	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Parsing of '%s' failed.", file_url);

	return error;
}
