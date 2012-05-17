/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#include "common.h"
#include "path.h"
#include "posix.h"
#ifdef GIT_WIN32
#include "win32/dir.h"
#include "win32/posix.h"
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

	/* Cast is safe because max path < max int */
	len = (int)(endp - startp + 1);

Exit:
	result = len;

	if (buffer != NULL && git_buf_set(buffer, startp, len) < 0)
		return -1;

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

	/* Cast is safe because max path < max int */
	len = (int)(endp - path + 1);

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

	if (buffer != NULL && git_buf_set(buffer, path, len) < 0)
		return -1;

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
	ssize_t i;

	assert(path);
	len = strlen(path);

	if (!len || path[len - 1] != '/')
		return NULL;

	for (i = (ssize_t)len - 2; i >= 0; --i)
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

	/* Are we dealing with a windows network path? */
	else if ((path[0] == '/' && path[1] == '/') ||
		(path[0] == '\\' && path[1] == '\\'))
	{
		offset += 2;

		/* Skip the computer name segment */
		while (path[offset] && path[offset] != '/' && path[offset] != '\\')
			offset++;
	}
#endif

	if (path[offset] == '/' || path[offset] == '\\')
		return offset;

	return -1;	/* Not a real error - signals that path is not rooted */
}

int git_path_prettify(git_buf *path_out, const char *path, const char *base)
{
	char buf[GIT_PATH_MAX];

	assert(path && path_out);

	/* construct path if needed */
	if (base != NULL && git_path_root(path) < 0) {
		if (git_buf_joinpath(path_out, base, path) < 0)
			return -1;
		path = path_out->ptr;
	}

	if (p_realpath(path, buf) == NULL) {
		/* giterr_set resets the errno when dealing with a GITERR_OS kind of error */
		int error = (errno == ENOENT || errno == ENOTDIR) ? GIT_ENOTFOUND : -1;
		giterr_set(GITERR_OS, "Failed to resolve path '%s'", path);

		git_buf_clear(path_out);
		
		return error;
	}

	return git_buf_sets(path_out, buf);
}

int git_path_prettify_dir(git_buf *path_out, const char *path, const char *base)
{
	int error = git_path_prettify(path_out, path, base);
	return (error < 0) ? error : git_path_to_dir(path_out);
}

int git_path_to_dir(git_buf *path)
{
	if (path->asize > 0 &&
		git_buf_len(path) > 0 &&
		path->ptr[git_buf_len(path) - 1] != '/')
		git_buf_putc(path, '/');

	return git_buf_oom(path) ? -1 : 0;
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
	int len, hi, lo, i;
	assert(decoded_out && input);

	len = (int)strlen(input);
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
		if (git_buf_putc(decoded_out, c) < 0)
			return -1;
	}

	return 0;
}

static int error_invalid_local_file_uri(const char *uri)
{
	giterr_set(GITERR_CONFIG, "'%s' is not a valid local file URI", uri);
	return -1;
}

int git_path_fromurl(git_buf *local_path_out, const char *file_url)
{
	int offset = 0, len;

	assert(local_path_out && file_url);

	if (git__prefixcmp(file_url, "file://") != 0)
		return error_invalid_local_file_uri(file_url);

	offset += 7;
	len = (int)strlen(file_url);

	if (offset < len && file_url[offset] == '/')
		offset++;
	else if (offset < len && git__prefixcmp(file_url + offset, "localhost/") == 0)
		offset += 10;
	else
		return error_invalid_local_file_uri(file_url);

	if (offset >= len || file_url[offset] == '/')
		return error_invalid_local_file_uri(file_url);

#ifndef _MSC_VER
	offset--;	/* A *nix absolute path starts with a forward slash */
#endif

	git_buf_clear(local_path_out);

	return git__percent_decode(local_path_out, file_url + offset);
}

int git_path_walk_up(
	git_buf *path,
	const char *ceiling,
	int (*cb)(void *data, git_buf *),
	void *data)
{
	int error = 0;
	git_buf iter;
	ssize_t stop = 0, scan;
	char oldc = '\0';

	assert(path && cb);

	if (ceiling != NULL) {
		if (git__prefixcmp(path->ptr, ceiling) == 0)
			stop = (ssize_t)strlen(ceiling);
		else
			stop = git_buf_len(path);
	}
	scan = git_buf_len(path);

	iter.ptr = path->ptr;
	iter.size = git_buf_len(path);
	iter.asize = path->asize;

	while (scan >= stop) {
		if ((error = cb(data, &iter)) < 0)
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

bool git_path_exists(const char *path)
{
	assert(path);
	return p_access(path, F_OK) == 0;
}

bool git_path_isdir(const char *path)
{
	struct stat st;
	if (p_stat(path, &st) < 0)
		return false;

	return S_ISDIR(st.st_mode) != 0;
}

bool git_path_isfile(const char *path)
{
	struct stat st;

	assert(path);
	if (p_stat(path, &st) < 0)
		return false;

	return S_ISREG(st.st_mode) != 0;
}

int git_path_lstat(const char *path, struct stat *st)
{
	int err = 0;

	if (p_lstat(path, st) < 0) {
		err = (errno == ENOENT) ? GIT_ENOTFOUND : -1;
		giterr_set(GITERR_OS, "Failed to stat file '%s'", path);
	}

	return err;
}

static bool _check_dir_contents(
	git_buf *dir,
	const char *sub,
	bool (*predicate)(const char *))
{
	bool result;
	size_t dir_size = git_buf_len(dir);
	size_t sub_size = strlen(sub);

	/* leave base valid even if we could not make space for subdir */
	if (git_buf_try_grow(dir, dir_size + sub_size + 2) < 0)
		return false;

	/* save excursion */
	git_buf_joinpath(dir, dir->ptr, sub);

	result = predicate(dir->ptr);

	/* restore path */
	git_buf_truncate(dir, dir_size);
	return result;
}

bool git_path_contains(git_buf *dir, const char *item)
{
	return _check_dir_contents(dir, item, &git_path_exists);
}

bool git_path_contains_dir(git_buf *base, const char *subdir)
{
	return _check_dir_contents(base, subdir, &git_path_isdir);
}

bool git_path_contains_file(git_buf *base, const char *file)
{
	return _check_dir_contents(base, file, &git_path_isfile);
}

int git_path_find_dir(git_buf *dir, const char *path, const char *base)
{
	int error;

	if (base != NULL && git_path_root(path) < 0)
		error = git_buf_joinpath(dir, base, path);
	else
		error = git_buf_sets(dir, path);

	if (!error) {
		char buf[GIT_PATH_MAX];
		if (p_realpath(dir->ptr, buf) != NULL)
			error = git_buf_sets(dir, buf);
	}

	/* call dirname if this is not a directory */
	if (!error && git_path_isdir(dir->ptr) == false)
		error = git_path_dirname_r(dir, dir->ptr);

	if (!error)
		error = git_path_to_dir(dir);

	return error;
}

int git_path_cmp(
	const char *name1, size_t len1, int isdir1,
	const char *name2, size_t len2, int isdir2)
{
	size_t len = len1 < len2 ? len1 : len2;
	int cmp;

	cmp = memcmp(name1, name2, len);
	if (cmp)
		return cmp;
	if (len1 < len2)
		return (!isdir1 && !isdir2) ? -1 :
			(isdir1 ? '/' - name2[len1] : name2[len1] - '/');
	if (len1 > len2)
		return (!isdir1 && !isdir2) ? 1 :
			(isdir2 ? name1[len2] - '/' : '/' - name1[len2]);
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
	struct dirent *de, *de_buf;

	if (git_path_to_dir(path) < 0)
		return -1;

	wd_len = git_buf_len(path);

	if ((dir = opendir(path->ptr)) == NULL) {
		giterr_set(GITERR_OS, "Failed to open directory '%s'", path->ptr);
		return -1;
	}

#ifdef __sun
	de_buf = git__malloc(sizeof(struct dirent) + FILENAME_MAX + 1);
#else
	de_buf = git__malloc(sizeof(struct dirent));
#endif

	while (p_readdir_r(dir, de_buf, &de) == 0 && de != NULL) {
		int result;

		if (is_dot_or_dotdot(de->d_name))
			continue;

		if (git_buf_puts(path, de->d_name) < 0) {
			closedir(dir);
			git__free(de_buf);
			return -1;
		}

		result = fn(arg, path);

		git_buf_truncate(path, wd_len); /* restore path */

		if (result < 0) {
			closedir(dir);
			git__free(de_buf);
			return -1;
		}
	}

	closedir(dir);
	git__free(de_buf);
	return 0;
}

int git_path_dirload(
	const char *path,
	size_t prefix_len,
	size_t alloc_extra,
	git_vector *contents)
{
	int error, need_slash;
	DIR *dir;
	struct dirent *de, *de_buf;
	size_t path_len;

	assert(path != NULL && contents != NULL);
	path_len = strlen(path);
	assert(path_len > 0 && path_len >= prefix_len);

	if ((dir = opendir(path)) == NULL) {
		giterr_set(GITERR_OS, "Failed to open directory '%s'", path);
		return -1;
	}

#ifdef __sun
	de_buf = git__malloc(sizeof(struct dirent) + FILENAME_MAX + 1);
#else
	de_buf = git__malloc(sizeof(struct dirent));
#endif

	path += prefix_len;
	path_len -= prefix_len;
	need_slash = (path_len > 0 && path[path_len-1] != '/') ? 1 : 0;

	while ((error = p_readdir_r(dir, de_buf, &de)) == 0 && de != NULL) {
		char *entry_path;
		size_t entry_len;

		if (is_dot_or_dotdot(de->d_name))
			continue;

		entry_len = strlen(de->d_name);

		entry_path = git__malloc(
			path_len + need_slash + entry_len + 1 + alloc_extra);
		GITERR_CHECK_ALLOC(entry_path);

		if (path_len)
			memcpy(entry_path, path, path_len);
		if (need_slash)
			entry_path[path_len] = '/';
		memcpy(&entry_path[path_len + need_slash], de->d_name, entry_len);
		entry_path[path_len + need_slash + entry_len] = '\0';

		if (git_vector_insert(contents, entry_path) < 0) {
			closedir(dir);
			git__free(de_buf);
			return -1;
		}
	}

	closedir(dir);
	git__free(de_buf);

	if (error != 0)
		giterr_set(GITERR_OS, "Failed to process directory entry in '%s'", path);

	return error;
}

int git_path_with_stat_cmp(const void *a, const void *b)
{
	const git_path_with_stat *psa = a, *psb = b;
	return git__strcmp_cb(psa->path, psb->path);
}

int git_path_dirload_with_stat(
	const char *path,
	size_t prefix_len,
	git_vector *contents)
{
	int error;
	unsigned int i;
	git_path_with_stat *ps;
	git_buf full = GIT_BUF_INIT;

	if (git_buf_set(&full, path, prefix_len) < 0)
		return -1;

	error = git_path_dirload(
		path, prefix_len, sizeof(git_path_with_stat) + 1, contents);
	if (error < 0) {
		git_buf_free(&full);
		return error;
	}

	git_vector_foreach(contents, i, ps) {
		size_t path_len = strlen((char *)ps);

		memmove(ps->path, ps, path_len + 1);
		ps->path_len = path_len;

		if ((error = git_buf_joinpath(&full, full.ptr, ps->path)) < 0 ||
			(error = git_path_lstat(full.ptr, &ps->st)) < 0)
			break;

		git_buf_truncate(&full, prefix_len);

		if (S_ISDIR(ps->st.st_mode)) {
			ps->path[path_len] = '/';
			ps->path[path_len + 1] = '\0';
		}
	}

	git_buf_free(&full);

	return error;
}
