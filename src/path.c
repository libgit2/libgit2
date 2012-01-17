/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#include "common.h"
#include "path.h"
#include "posix.h"
#ifdef GIT_WIN32
#include "win32/dir.h"
#else
#include <dirent.h>
#endif
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

int git_path_walk_up(
	git_buf *path,
	const char *ceiling,
	int (*cb)(void *data, git_buf *),
	void *data)
{
	int error = GIT_SUCCESS;
	git_buf iter;
	ssize_t stop = 0, scan;
	char oldc = '\0';

	assert(path && cb);

	if (ceiling != NULL) {
		if (git__prefixcmp(path->ptr, ceiling) == GIT_SUCCESS)
			stop = (ssize_t)strlen(ceiling);
		else
			stop = path->size;
	}
	scan = path->size;

	iter.ptr = path->ptr;
	iter.size = path->size;
	iter.asize = path->asize;

	while (scan >= stop) {
		if ((error = cb(data, &iter)) < GIT_SUCCESS)
			break;
		iter.ptr[scan] = oldc;
		scan = git_buf_rfind_next(&iter, '/');
		if (scan >= 0) {
			scan++;
			oldc = iter.ptr[scan];
			iter.size = scan;
			iter.ptr[scan] = '\0';
		}
	}

	if (scan >= 0)
		iter.ptr[scan] = oldc;

	return error;
}

int git_path_exists(const char *path)
{
	assert(path);
	return p_access(path, F_OK);
}

int git_path_isdir(const char *path)
{
#ifdef GIT_WIN32
	DWORD attr = GetFileAttributes(path);
	if (attr == INVALID_FILE_ATTRIBUTES)
		return GIT_ERROR;

	return (attr & FILE_ATTRIBUTE_DIRECTORY) ? GIT_SUCCESS : GIT_ERROR;

#else
	struct stat st;
	if (p_stat(path, &st) < GIT_SUCCESS)
		return GIT_ERROR;

	return S_ISDIR(st.st_mode) ? GIT_SUCCESS : GIT_ERROR;
#endif
}

int git_path_isfile(const char *path)
{
	struct stat st;
	int stat_error;

	assert(path);
	stat_error = p_stat(path, &st);

	if (stat_error < GIT_SUCCESS)
		return -1;

	if (!S_ISREG(st.st_mode))
		return -1;

	return 0;
}

static int _check_dir_contents(
	git_buf *dir,
	const char *sub,
	int append_on_success,
	int (*predicate)(const char *))
{
	int error = GIT_SUCCESS;
	size_t dir_size = dir->size;
	size_t sub_size = strlen(sub);

	/* leave base valid even if we could not make space for subdir */
	if ((error = git_buf_try_grow(dir, dir_size + sub_size + 2)) < GIT_SUCCESS)
		return error;

	/* save excursion */
	git_buf_joinpath(dir, dir->ptr, sub);

	error = (*predicate)(dir->ptr);

	/* restore excursion */
	if (!append_on_success || error != GIT_SUCCESS)
		git_buf_truncate(dir, dir_size);

	return error;
}

int git_path_contains_dir(git_buf *base, const char *subdir, int append_if_exists)
{
	return _check_dir_contents(base, subdir, append_if_exists, &git_path_isdir);
}

int git_path_contains_file(git_buf *base, const char *file, int append_if_exists)
{
	return _check_dir_contents(base, file, append_if_exists, &git_path_isfile);
}

int git_path_find_dir(git_buf *dir, const char *path, const char *base)
{
	int error = GIT_SUCCESS;

	if (base != NULL && git_path_root(path) < 0)
		error = git_buf_joinpath(dir, base, path);
	else
		error = git_buf_sets(dir, path);

	if (error == GIT_SUCCESS) {
		char buf[GIT_PATH_MAX];
		if (p_realpath(dir->ptr, buf) != NULL)
			error = git_buf_sets(dir, buf);
	}

	/* call dirname if this is not a directory */
	if (error == GIT_SUCCESS && git_path_isdir(dir->ptr) != GIT_SUCCESS)
		if (git_path_dirname_r(dir, dir->ptr) < GIT_SUCCESS)
			error = git_buf_lasterror(dir);

	if (error == GIT_SUCCESS)
		error = git_path_to_dir(dir);

	return error;
}

int git_path_cmp(const char *name1, int len1, int isdir1,
		const char *name2, int len2, int isdir2)
{
	int len = len1 < len2 ? len1 : len2;
	int cmp;

	cmp = memcmp(name1, name2, len);
	if (cmp)
		return cmp;
	if (len1 < len2)
		return ((!isdir1 && !isdir2) ? -1 :
						(isdir1 ? '/' - name2[len1] : name2[len1] - '/'));
	if (len1 > len2)
		return ((!isdir1 && !isdir2) ? 1 :
						(isdir2 ? name1[len2] - '/' : '/' - name1[len2]));
	return 0;
}

/* Taken from git.git */
GIT_INLINE(int) is_dot_or_dotdot(const char *name)
{
	return (name[0] == '.' &&
		(name[1] == '\0' ||
		 (name[1] == '.' && name[2] == '\0')));
}

int git_path_direach(
	git_buf *path,
	int (*fn)(void *, git_buf *),
	void *arg)
{
	ssize_t wd_len;
	DIR *dir;
	struct dirent *de;

	if (git_path_to_dir(path) < GIT_SUCCESS)
		return git_buf_lasterror(path);

	wd_len = path->size;
	dir = opendir(path->ptr);
	if (!dir)
		return git__throw(GIT_EOSERR, "Failed to process `%s` tree structure. An error occured while opening the directory", path->ptr);

	while ((de = readdir(dir)) != NULL) {
		int result;

		if (is_dot_or_dotdot(de->d_name))
			continue;

		if (git_buf_puts(path, de->d_name) < GIT_SUCCESS)
			return git_buf_lasterror(path);

		result = fn(arg, path);

		git_buf_truncate(path, wd_len); /* restore path */

		if (result != GIT_SUCCESS) {
			closedir(dir);
			return result;	/* The callee is reponsible for setting the correct error message */
		}
	}

	closedir(dir);
	return GIT_SUCCESS;
}
