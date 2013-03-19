/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
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

#define LOOKS_LIKE_DRIVE_PREFIX(S) (git__isalpha((S)[0]) && (S)[1] == ':')

#ifdef GIT_WIN32
static bool looks_like_network_computer_name(const char *path, int pos)
{
	if (pos < 3)
		return false;

	if (path[0] != '/' || path[1] != '/')
		return false;

	while (pos-- > 2) {
		if (path[pos] == '/')
			return false;
	}

	return true;
}
#endif

/*
 * Based on the Android implementation, BSD licensed.
 * http://android.git.kernel.org/
 *
 * Copyright (C) 2008 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * AS IS AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
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

	if (len == 2 && LOOKS_LIKE_DRIVE_PREFIX(path)) {
		len = 3;
		goto Exit;
	}

	/* Similarly checks if we're dealing with a network computer name
		'//computername/.git' will return '//computername/' */

	if (looks_like_network_computer_name(path, len)) {
		len++;
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

size_t git_path_basename_offset(git_buf *buffer)
{
	ssize_t slash;

	if (!buffer || buffer->size <= 0)
		return 0;

	slash = git_buf_rfind_next(buffer, '/');

	if (slash >= 0 && buffer->ptr[slash] == '/')
		return (size_t)(slash + 1);

	return 0;
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

	/* Does the root of the path look like a windows drive ? */
	if (LOOKS_LIKE_DRIVE_PREFIX(path))
		offset += 2;

#ifdef GIT_WIN32
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

int git_path_join_unrooted(
	git_buf *path_out, const char *path, const char *base, ssize_t *root_at)
{
	int error, root;

	assert(path && path_out);

	root = git_path_root(path);

	if (base != NULL && root < 0) {
		error = git_buf_joinpath(path_out, base, path);

		if (root_at)
			*root_at = (ssize_t)strlen(base);
	}
	else {
		error = git_buf_sets(path_out, path);

		if (root_at)
			*root_at = (root < 0) ? 0 : (ssize_t)root;
	}

	return error;
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

#ifndef GIT_WIN32
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
		error = cb(data, &iter);
		iter.ptr[scan] = oldc;
		if (error < 0)
			break;
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

#ifdef GIT_WIN32

bool git_path_is_empty_dir(const char *path)
{
	git_buf pathbuf = GIT_BUF_INIT;
	HANDLE hFind = INVALID_HANDLE_VALUE;
	wchar_t wbuf[GIT_WIN_PATH];
	WIN32_FIND_DATAW ffd;
	bool retval = true;

	if (!git_path_isdir(path)) return false;

	git_buf_printf(&pathbuf, "%s\\*", path);
	git__utf8_to_16(wbuf, GIT_WIN_PATH, git_buf_cstr(&pathbuf));

	hFind = FindFirstFileW(wbuf, &ffd);
	if (INVALID_HANDLE_VALUE == hFind) {
		giterr_set(GITERR_OS, "Couldn't open '%s'", path);
		return false;
	}

	do {
		if (!git_path_is_dot_or_dotdotW(ffd.cFileName)) {
			retval = false;
		}
	} while (FindNextFileW(hFind, &ffd) != 0);

	FindClose(hFind);
	git_buf_free(&pathbuf);
	return retval;
}

#else

bool git_path_is_empty_dir(const char *path)
{
	DIR *dir = NULL;
	struct dirent *e;
	bool retval = true;

	if (!git_path_isdir(path)) return false;

	dir = opendir(path);
	if (!dir) {
		giterr_set(GITERR_OS, "Couldn't open '%s'", path);
		return false;
	}

	while ((e = readdir(dir)) != NULL) {
		if (!git_path_is_dot_or_dotdot(e->d_name)) {
			giterr_set(GITERR_INVALID,
						  "'%s' exists and is not an empty directory", path);
			retval = false;
			break;
		}
	}
	closedir(dir);

	return retval;
}
#endif

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
	if (git_buf_try_grow(dir, dir_size + sub_size + 2, false) < 0)
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
	int error = git_path_join_unrooted(dir, path, base, NULL);

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

int git_path_resolve_relative(git_buf *path, size_t ceiling)
{
	char *base, *to, *from, *next;
	size_t len;

	if (!path || git_buf_oom(path))
		return -1;

	if (ceiling > path->size)
		ceiling = path->size;

	/* recognize drive prefixes, etc. that should not be backed over */
	if (ceiling == 0)
		ceiling = git_path_root(path->ptr) + 1;

	/* recognize URL prefixes that should not be backed over */
	if (ceiling == 0) {
		for (next = path->ptr; *next && git__isalpha(*next); ++next);
		if (next[0] == ':' && next[1] == '/' && next[2] == '/')
			ceiling = (next + 3) - path->ptr;
	}

	base = to = from = path->ptr + ceiling;

	while (*from) {
		for (next = from; *next && *next != '/'; ++next);

		len = next - from;

		if (len == 1 && from[0] == '.')
			/* do nothing with singleton dot */;

		else if (len == 2 && from[0] == '.' && from[1] == '.') {
			while (to > base && to[-1] == '/') to--;
			while (to > base && to[-1] != '/') to--;
		}

		else {
			if (*next == '/')
				len++;

			if (to != from)
				memmove(to, from, len);

			to += len;
		}

		from += len;

		while (*from == '/') from++;
	}

	*to = '\0';

	path->size = to - path->ptr;

	return 0;
}

int git_path_apply_relative(git_buf *target, const char *relpath)
{
	git_buf_joinpath(target, git_buf_cstr(target), relpath);
	return git_path_resolve_relative(target, 0);
}

int git_path_cmp(
	const char *name1, size_t len1, int isdir1,
	const char *name2, size_t len2, int isdir2,
	int (*compare)(const char *, const char *, size_t))
{
	unsigned char c1, c2;
	size_t len = len1 < len2 ? len1 : len2;
	int cmp;

	cmp = compare(name1, name2, len);
	if (cmp)
		return cmp;

	c1 = name1[len];
	c2 = name2[len];

	if (c1 == '\0' && isdir1)
		c1 = '/';

	if (c2 == '\0' && isdir2)
		c2 = '/';

	return (c1 < c2) ? -1 : (c1 > c2) ? 1 : 0;
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

#if defined(__sun) || defined(__GNU__)
	de_buf = git__malloc(sizeof(struct dirent) + FILENAME_MAX + 1);
#else
	de_buf = git__malloc(sizeof(struct dirent));
#endif

	while (p_readdir_r(dir, de_buf, &de) == 0 && de != NULL) {
		int result;

		if (git_path_is_dot_or_dotdot(de->d_name))
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

#if defined(__sun) || defined(__GNU__)
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

		if (git_path_is_dot_or_dotdot(de->d_name))
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
	return strcmp(psa->path, psb->path);
}

int git_path_with_stat_cmp_icase(const void *a, const void *b)
{
	const git_path_with_stat *psa = a, *psb = b;
	return strcasecmp(psa->path, psb->path);
}

int git_path_dirload_with_stat(
	const char *path,
	size_t prefix_len,
	bool ignore_case,
	const char *start_stat,
	const char *end_stat,
	git_vector *contents)
{
	int error;
	unsigned int i;
	git_path_with_stat *ps;
	git_buf full = GIT_BUF_INIT;
	int (*strncomp)(const char *a, const char *b, size_t sz);
	size_t start_len = start_stat ? strlen(start_stat) : 0;
	size_t end_len = end_stat ? strlen(end_stat) : 0, cmp_len;

	if (git_buf_set(&full, path, prefix_len) < 0)
		return -1;

	error = git_path_dirload(
		path, prefix_len, sizeof(git_path_with_stat) + 1, contents);
	if (error < 0) {
		git_buf_free(&full);
		return error;
	}

	strncomp = ignore_case ? git__strncasecmp : git__strncmp;

	/* stat struct at start of git_path_with_stat, so shift path text */
	git_vector_foreach(contents, i, ps) {
		size_t path_len = strlen((char *)ps);
		memmove(ps->path, ps, path_len + 1);
		ps->path_len = path_len;
	}

	git_vector_foreach(contents, i, ps) {
		/* skip if before start_stat or after end_stat */
		cmp_len = min(start_len, ps->path_len);
		if (cmp_len && strncomp(ps->path, start_stat, cmp_len) < 0)
			continue;
		cmp_len = min(end_len, ps->path_len);
		if (cmp_len && strncomp(ps->path, end_stat, cmp_len) > 0)
			continue;

		git_buf_truncate(&full, prefix_len);

		if ((error = git_buf_joinpath(&full, full.ptr, ps->path)) < 0 ||
			(error = git_path_lstat(full.ptr, &ps->st)) < 0)
			break;

		if (S_ISDIR(ps->st.st_mode)) {
			if ((error = git_buf_joinpath(&full, full.ptr, ".git")) < 0)
				break;

			if (p_access(full.ptr, F_OK) == 0) {
				ps->st.st_mode = GIT_FILEMODE_COMMIT;
			} else {
				ps->path[ps->path_len++] = '/';
				ps->path[ps->path_len] = '\0';
			}
		}
	}

	/* sort now that directory suffix is added */
	git_vector_sort(contents);

	git_buf_free(&full);

	return error;
}
