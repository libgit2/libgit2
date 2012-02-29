/*
 * Copyright (C) 2009-2012 the libgit2 contributors
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
	if (git_path_isdir(target_folder.ptr) != GIT_SUCCESS)
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

git_off_t git_futils_filesize(git_file fd)
{
	struct stat sb;
	if (p_fstat(fd, &sb))
		return GIT_ERROR;

	return sb.st_size;
}

mode_t git_futils_canonical_mode(mode_t raw_mode)
{
	if (S_ISREG(raw_mode))
		return S_IFREG | GIT_CANONICAL_PERMS(raw_mode);
	else if (S_ISLNK(raw_mode))
		return S_IFLNK;
	else if (S_ISDIR(raw_mode))
		return S_IFDIR;
	else if (S_ISGITLINK(raw_mode))
		return S_IFGITLINK;
	else
		return 0;
}

int git_futils_readbuffer_updated(git_buf *buf, const char *path, time_t *mtime, int *updated)
{
	git_file fd;
	size_t len;
	struct stat st;

	assert(buf && path && *path);

	if (updated != NULL)
		*updated = 0;

	if ((fd = p_open(path, O_RDONLY)) < 0) {
		return git__throw(GIT_ENOTFOUND, "Failed to read file '%s': %s", path, strerror(errno));
	}

	if (p_fstat(fd, &st) < 0 || S_ISDIR(st.st_mode) || !git__is_sizet(st.st_size+1)) {
		close(fd);
		return git__throw(GIT_EOSERR, "Failed to stat file '%s'", path);
	}

	/*
	 * If we were given a time, we only want to read the file if it
	 * has been modified.
	 */
	if (mtime != NULL && *mtime >= st.st_mtime) {
		close(fd);
		return 0;
	}

	if (mtime != NULL)
		*mtime = st.st_mtime;

	len = (size_t) st.st_size;

	git_buf_clear(buf);

	if (git_buf_grow(buf, len + 1) < 0) {
		close(fd);
		return GIT_ENOMEM;
	}

	buf->ptr[len] = '\0';

	while (len > 0) {
		ssize_t read_size = p_read(fd, buf->ptr, len);

		if (read_size < 0) {
			close(fd);
			return git__throw(GIT_EOSERR, "Failed to read from FD");
		}

		len -= read_size;
		buf->size += read_size;
	}

	p_close(fd);

	if (mtime != NULL)
		*mtime = st.st_mtime;

	if (updated != NULL)
		*updated = 1;

	return 0;
}

int git_futils_readbuffer(git_buf *buf, const char *path)
{
	return git_futils_readbuffer_updated(buf, path, NULL, NULL);
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

int git_futils_mmap_ro_file(git_map *out, const char *path)
{
	git_file fd = p_open(path, O_RDONLY /* | O_NOATIME */);
	git_off_t len = git_futils_filesize(fd);
	int result;
	if (!git__is_sizet(len))
		return git__throw(GIT_ERROR, "File `%s` too large to mmap", path);
	result = git_futils_mmap_ro(out, fd, 0, (size_t)len);
	p_close(fd);
	return result;
}

void git_futils_mmap_free(git_map *out)
{
	p_munmap(out);
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
		if (sp != pp && git_path_isdir(make_path.ptr) < GIT_SUCCESS) {
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

	if (git_path_isdir(path->ptr) == GIT_SUCCESS) {
		error = git_path_direach(path, _rmdir_recurs_foreach, opaque);
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

int git_futils_find_global_file(git_buf *path, const char *filename)
{
	int error;
	const char *home = getenv("HOME");

#ifdef GIT_WIN32
	if (home == NULL)
		home = getenv("USERPROFILE");
#endif

	if (home == NULL)
		return git__throw(GIT_EOSERR, "Failed to open global %s file. "
			"Cannot locate the user's home directory.", filename);

	if ((error = git_buf_joinpath(path, home, filename)) < GIT_SUCCESS)
		return error;

	if (git_path_exists(path->ptr) < GIT_SUCCESS) {
		git_buf_clear(path);
		return GIT_ENOTFOUND;
	}

	return GIT_SUCCESS;
}

#ifdef GIT_WIN32
typedef struct {
	wchar_t *path;
	DWORD len;
} win32_path;

static const win32_path *win32_system_root(void)
{
	static win32_path s_root = { 0, 0 };

	if (s_root.path == NULL) {
		const wchar_t *root_tmpl = L"%PROGRAMFILES%\\Git\\etc\\";

		s_root.len = ExpandEnvironmentStringsW(root_tmpl, NULL, 0);

		if (s_root.len <= 0) {
			git__throw(GIT_EOSERR, "Failed to expand environment strings");
			return NULL;
		}

		s_root.path = git__calloc(s_root.len, sizeof(wchar_t));
		if (s_root.path == NULL)
			return NULL;

		if (ExpandEnvironmentStringsW(root_tmpl, s_root.path, s_root.len) != s_root.len) {
			git__throw(GIT_EOSERR, "Failed to expand environment strings");
			git__free(s_root.path);
			s_root.path = NULL;
			return NULL;
		}
	}

	return &s_root;
}

static int win32_find_system_file(git_buf *path, const char *filename)
{
	int error = GIT_SUCCESS;
	const win32_path *root = win32_system_root();
	size_t len;
	wchar_t *file_utf16 = NULL, *scan;
	char *file_utf8 = NULL;

	if (!root || !filename || (len = strlen(filename)) == 0)
		return GIT_ENOTFOUND;

	/* allocate space for wchar_t path to file */
	file_utf16 = git__calloc(root->len + len + 2, sizeof(wchar_t));
	if (!file_utf16)
		return GIT_ENOMEM;

	/* append root + '\\' + filename as wchar_t */
	memcpy(file_utf16, root->path, root->len * sizeof(wchar_t));

	if (*filename == '/' || *filename == '\\')
		filename++;

	if (gitwin_append_utf16(file_utf16 + root->len - 1, filename, len + 1) !=
		(int)len + 1) {
		error = git__throw(GIT_EOSERR, "Failed to build file path");
		goto cleanup;
	}

	for (scan = file_utf16; *scan; scan++)
		if (*scan == L'/')
			*scan = L'\\';

	/* check access */
	if (_waccess(file_utf16, F_OK) < 0) {
		error = GIT_ENOTFOUND;
		goto cleanup;
	}

	/* convert to utf8 */
	if ((file_utf8 = gitwin_from_utf16(file_utf16)) == NULL)
		error = GIT_ENOMEM;

	if (file_utf8) {
		git_path_mkposix(file_utf8);
		git_buf_attach(path, file_utf8, 0);
	}

cleanup:
	git__free(file_utf16);

	return error;
}
#endif

int git_futils_find_system_file(git_buf *path, const char *filename)
{
	if (git_buf_joinpath(path, "/etc", filename) < GIT_SUCCESS)
		return git_buf_lasterror(path);

	if (git_path_exists(path->ptr) == GIT_SUCCESS)
		return GIT_SUCCESS;

	git_buf_clear(path);

#ifdef GIT_WIN32
	return win32_find_system_file(path, filename);
#else
	return GIT_ENOTFOUND;
#endif
}
