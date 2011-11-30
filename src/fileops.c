/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#include "common.h"
#include "fileops.h"
#include <ctype.h>

int git_futils_mkpath2file(const char *file_path, const mode_t mode)
{
	int error;
	git_buf target_folder = GIT_BUF_INIT;

	error = git_path_dirname_r(&target_folder, file_path);
	if (error < GIT_SUCCESS) {
		git_buf_free(&target_folder);
		return git__throw(GIT_EINVALIDPATH, "Failed to recursively build `%s` tree structure. Unable to parse parent folder name", file_path);
	} else {
		/* reset error */
		error = GIT_SUCCESS;
	}

	/* Does the containing folder exist? */
	if (git_futils_isdir(target_folder.ptr) != GIT_SUCCESS)
		/* Let's create the tree structure */
		error = git_futils_mkdir_r(target_folder.ptr, NULL, mode);

	git_buf_free(&target_folder);
	return error;
}

int git_futils_mktmp(git_buf *path_out, const char *filename)
{
	int fd;

	git_buf_sets(path_out, filename);
	git_buf_puts(path_out, "_git2_XXXXXX");

	if (git_buf_oom(path_out))
		return git__rethrow(git_buf_lasterror(path_out),
			"Failed to create temporary file for %s", filename);

	if ((fd = p_mkstemp(path_out->ptr)) < 0)
		return git__throw(GIT_EOSERR, "Failed to create temporary file %s", path_out->ptr);

	return fd;
}

int git_futils_creat_withpath(const char *path, const mode_t dirmode, const mode_t mode)
{
	if (git_futils_mkpath2file(path, dirmode) < GIT_SUCCESS)
		return git__throw(GIT_EOSERR, "Failed to create file %s", path);

	return p_creat(path, mode);
}

int git_futils_creat_locked(const char *path, const mode_t mode)
{
	int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY | O_EXCL, mode);
	return fd >= 0 ? fd : git__throw(GIT_EOSERR, "Failed to create locked file. Could not open %s", path);
}

int git_futils_creat_locked_withpath(const char *path, const mode_t dirmode, const mode_t mode)
{
	if (git_futils_mkpath2file(path, dirmode) < GIT_SUCCESS)
		return git__throw(GIT_EOSERR, "Failed to create locked file %s", path);

	return git_futils_creat_locked(path, mode);
}

int git_futils_isdir(const char *path)
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

int git_futils_isfile(const char *path)
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

int git_futils_exists(const char *path)
{
	assert(path);
	return p_access(path, F_OK);
}

git_off_t git_futils_filesize(git_file fd)
{
	struct stat sb;
	if (p_fstat(fd, &sb))
		return GIT_ERROR;

	return sb.st_size;
}

int git_futils_readbuffer_updated(git_fbuffer *obj, const char *path, time_t *mtime, int *updated)
{
	git_file fd;
	size_t len;
	struct stat st;
	unsigned char *buff;

	assert(obj && path && *path);

	if (updated != NULL)
		*updated = 0;

	if (p_stat(path, &st) < 0)
		return git__throw(GIT_ENOTFOUND, "Failed to stat file %s", path);

	if (S_ISDIR(st.st_mode))
		return git__throw(GIT_ERROR, "Can't read a dir into a buffer");

	/*
	 * If we were given a time, we only want to read the file if it
	 * has been modified.
	 */
	if (mtime != NULL && *mtime >= st.st_mtime)
		return GIT_SUCCESS;

	if (mtime != NULL)
		*mtime = st.st_mtime;
	if (!git__is_sizet(st.st_size+1))
		return git__throw(GIT_ERROR, "Failed to read file `%s`. An error occured while calculating its size", path);

	len = (size_t) st.st_size;

	if ((fd = p_open(path, O_RDONLY)) < 0)
		return git__throw(GIT_EOSERR, "Failed to open %s for reading", path);

	if ((buff = git__malloc(len + 1)) == NULL) {
		p_close(fd);
		return GIT_ENOMEM;
	}

	if (p_read(fd, buff, len) < 0) {
		p_close(fd);
		git__free(buff);
		return git__throw(GIT_ERROR, "Failed to read file `%s`", path);
	}
	buff[len] = '\0';

	p_close(fd);

	if (mtime != NULL)
		*mtime = st.st_mtime;
	if (updated != NULL)
		*updated = 1;

	obj->data = buff;
	obj->len = len;

	return GIT_SUCCESS;
}

int git_futils_readbuffer(git_fbuffer *obj, const char *path)
{
	return git_futils_readbuffer_updated(obj, path, NULL, NULL);
}

void git_futils_fbuffer_rtrim(git_fbuffer *obj)
{
	unsigned char *buff = obj->data;
	while (obj->len > 0 && isspace(buff[obj->len - 1]))
		obj->len--;
	buff[obj->len] = '\0';
}

void git_futils_freebuffer(git_fbuffer *obj)
{
	assert(obj);
	git__free(obj->data);
	obj->data = NULL;
}


int git_futils_mv_withpath(const char *from, const char *to, const mode_t dirmode)
{
	if (git_futils_mkpath2file(to, dirmode) < GIT_SUCCESS)
		return GIT_EOSERR;	/* The callee already takes care of setting the correct error message. */

	return p_rename(from, to); /* The callee already takes care of setting the correct error message. */
}

int git_futils_mmap_ro(git_map *out, git_file fd, git_off_t begin, size_t len)
{
	return p_mmap(out, len, GIT_PROT_READ, GIT_MAP_SHARED, fd, begin);
}

void git_futils_mmap_free(git_map *out)
{
	p_munmap(out);
}

/* Taken from git.git */
GIT_INLINE(int) is_dot_or_dotdot(const char *name)
{
	return (name[0] == '.' &&
		(name[1] == '\0' ||
		 (name[1] == '.' && name[2] == '\0')));
}

int git_futils_direach(
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

int git_futils_mkdir_r(const char *path, const char *base, const mode_t mode)
{
	int error, root_path_offset;
	git_buf make_path = GIT_BUF_INIT;
	size_t start;
	char *pp, *sp;

	if (base != NULL) {
		start = strlen(base);
		error = git_buf_joinpath(&make_path, base, path);
	} else {
		start = 0;
		error = git_buf_puts(&make_path, path);
	}
	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to create `%s` tree structure", path);

	pp = make_path.ptr + start;

	root_path_offset = git_path_root(make_path.ptr);
	if (root_path_offset > 0)
		pp += root_path_offset; /* On Windows, will skip the drive name (eg. C: or D:) */

	while (error == GIT_SUCCESS && (sp = strchr(pp, '/')) != NULL) {
		if (sp != pp && git_futils_isdir(make_path.ptr) < GIT_SUCCESS) {
			*sp = 0;
			error = p_mkdir(make_path.ptr, mode);

			/* Do not choke while trying to recreate an existing directory */
			if (errno == EEXIST)
				error = GIT_SUCCESS;

			*sp = '/';
		}

		pp = sp + 1;
	}

	if (*pp != '\0' && error == GIT_SUCCESS) {
		error = p_mkdir(make_path.ptr, mode);
		if (errno == EEXIST)
			error = GIT_SUCCESS;
	}

	git_buf_free(&make_path);

	if (error < GIT_SUCCESS)
		return git__throw(error, "Failed to recursively create `%s` tree structure", path);

	return GIT_SUCCESS;
}

static int _rmdir_recurs_foreach(void *opaque, git_buf *path)
{
	int error = GIT_SUCCESS;
	int force = *(int *)opaque;

	if (git_futils_isdir(path->ptr) == GIT_SUCCESS) {
		error = git_futils_direach(path, _rmdir_recurs_foreach, opaque);
		if (error < GIT_SUCCESS)
			return git__rethrow(error, "Failed to remove directory `%s`", path->ptr);
		return p_rmdir(path->ptr);

	} else if (force) {
		return p_unlink(path->ptr);
	}

	return git__rethrow(error, "Failed to remove directory. `%s` is not empty", path->ptr);
}

int git_futils_rmdir_r(const char *path, int force)
{
	int error;
	git_buf p = GIT_BUF_INIT;

	error = git_buf_sets(&p, path);
	if (error == GIT_SUCCESS)
		error = _rmdir_recurs_foreach(&force, &p);
	git_buf_free(&p);
	return error;
}

int git_futils_cmp_path(const char *name1, int len1, int isdir1,
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

int git_futils_contains_dir(git_buf *base, const char *subdir, int append_if_exists)
{
	return _check_dir_contents(base, subdir, append_if_exists, &git_futils_isdir);
}

int git_futils_contains_file(git_buf *base, const char *file, int append_if_exists)
{
	return _check_dir_contents(base, file, append_if_exists, &git_futils_isfile);
}

