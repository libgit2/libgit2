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
	int result = 0;
	git_buf target_folder = GIT_BUF_INIT;

	if (git_path_dirname_r(&target_folder, file_path) < 0)
		return -1;

	/* Does the containing folder exist? */
	if (git_path_isdir(target_folder.ptr) == false)
		/* Let's create the tree structure */
		result = git_futils_mkdir_r(target_folder.ptr, NULL, mode);

	git_buf_free(&target_folder);
	return result;
}

int git_futils_mktmp(git_buf *path_out, const char *filename)
{
	int fd;

	git_buf_sets(path_out, filename);
	git_buf_puts(path_out, "_git2_XXXXXX");

	if (git_buf_oom(path_out))
		return -1;

	if ((fd = p_mkstemp(path_out->ptr)) < 0) {
		giterr_set(GITERR_OS,
			"Failed to create temporary file '%s'", path_out->ptr);
		return -1;
	}

	return fd;
}

int git_futils_creat_withpath(const char *path, const mode_t dirmode, const mode_t mode)
{
	int fd;

	if (git_futils_mkpath2file(path, dirmode) < 0)
		return -1;

	fd = p_creat(path, mode);
	if (fd < 0) {
		giterr_set(GITERR_OS, "Failed to create file '%s'", path);
		return -1;
	}

	return fd;
}

int git_futils_creat_locked(const char *path, const mode_t mode)
{
	int fd;

#ifdef GIT_WIN32
	wchar_t* buf;

	buf = gitwin_to_utf16(path);
	fd = _wopen(buf, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY | O_EXCL, mode);
	git__free(buf);
#else
	fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY | O_EXCL, mode);
#endif

	if (fd < 0) {
		giterr_set(GITERR_OS, "Failed to create locked file '%s'", path);
		return -1;
	}

	return fd;
}

int git_futils_creat_locked_withpath(const char *path, const mode_t dirmode, const mode_t mode)
{
	if (git_futils_mkpath2file(path, dirmode) < 0)
		return -1;

	return git_futils_creat_locked(path, mode);
}

int git_futils_open_ro(const char *path)
{
	int fd = p_open(path, O_RDONLY);
	if (fd < 0) {
		if (errno == ENOENT)
			fd = GIT_ENOTFOUND;
		giterr_set(GITERR_OS, "Failed to open '%s'", path);
	}
	return fd;
}

git_off_t git_futils_filesize(git_file fd)
{
	struct stat sb;

	if (p_fstat(fd, &sb)) {
		giterr_set(GITERR_OS, "Failed to stat file descriptor");
		return -1;
	}

	return sb.st_size;
}

mode_t git_futils_canonical_mode(mode_t raw_mode)
{
	if (S_ISREG(raw_mode))
		return S_IFREG | GIT_CANONICAL_PERMS(raw_mode);
	else if (S_ISLNK(raw_mode))
		return S_IFLNK;
	else if (S_ISGITLINK(raw_mode))
		return S_IFGITLINK;
	else if (S_ISDIR(raw_mode))
		return S_IFDIR;
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

	if ((fd = git_futils_open_ro(path)) < 0)
		return fd;

	if (p_fstat(fd, &st) < 0 || S_ISDIR(st.st_mode) || !git__is_sizet(st.st_size+1)) {
		p_close(fd);
		giterr_set(GITERR_OS, "Invalid regular file stat for '%s'", path);
		return -1;
	}

	/*
	 * If we were given a time, we only want to read the file if it
	 * has been modified.
	 */
	if (mtime != NULL && *mtime >= st.st_mtime) {
		p_close(fd);
		return 0;
	}

	if (mtime != NULL)
		*mtime = st.st_mtime;

	len = (size_t) st.st_size;

	git_buf_clear(buf);

	if (git_buf_grow(buf, len + 1) < 0) {
		p_close(fd);
		return -1;
	}

	buf->ptr[len] = '\0';

	while (len > 0) {
		ssize_t read_size = p_read(fd, buf->ptr, len);

		if (read_size < 0) {
			p_close(fd);
			giterr_set(GITERR_OS, "Failed to read descriptor for '%s'", path);
			return -1;
		}

		len -= read_size;
		buf->size += read_size;
	}

	p_close(fd);

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
	if (git_futils_mkpath2file(to, dirmode) < 0)
		return -1;

	if (p_rename(from, to) < 0) {
		giterr_set(GITERR_OS, "Failed to rename '%s' to '%s'", from, to);
		return -1;
	}

	return 0;
}

int git_futils_mmap_ro(git_map *out, git_file fd, git_off_t begin, size_t len)
{
	return p_mmap(out, len, GIT_PROT_READ, GIT_MAP_SHARED, fd, begin);
}

int git_futils_mmap_ro_file(git_map *out, const char *path)
{
	git_file fd = git_futils_open_ro(path);
	git_off_t len;
	int result;

	if (fd < 0)
		return fd;

	len = git_futils_filesize(fd);
	if (!git__is_sizet(len)) {
		giterr_set(GITERR_OS, "File `%s` too large to mmap", path);
		return -1;
	}

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
	git_buf make_path = GIT_BUF_INIT;
	size_t start = 0;
	char *pp, *sp;
	bool failed = false;

	if (base != NULL) {
		/*
		 * when a base is being provided, it is supposed to already exist.
		 * Therefore, no attempt is being made to recursively create this leading path
		 * segment. It's just skipped. */
		start = strlen(base);
		if (git_buf_joinpath(&make_path, base, path) < 0)
			return -1;
	} else {
		int root_path_offset;

		if (git_buf_puts(&make_path, path) < 0)
			return -1;

		root_path_offset = git_path_root(make_path.ptr);
		if (root_path_offset > 0) {
			 /*
			  * On Windows, will skip the drive name (eg. C: or D:)
			  * or the leading part of a network path (eg. //computer_name ) */
			start = root_path_offset;
		}
	}

	pp = make_path.ptr + start;

	while (!failed && (sp = strchr(pp, '/')) != NULL) {
		if (sp != pp && git_path_isdir(make_path.ptr) == false) {
			*sp = 0;

			/* Do not choke while trying to recreate an existing directory */
			if (p_mkdir(make_path.ptr, mode) < 0 && errno != EEXIST)
				failed = true;

			*sp = '/';
		}

		pp = sp + 1;
	}

	if (*pp != '\0' && !failed) {
		if (p_mkdir(make_path.ptr, mode) < 0 && errno != EEXIST)
			failed = true;
	}

	git_buf_free(&make_path);

	if (failed) {
		giterr_set(GITERR_OS,
			"Failed to create directory structure at '%s'", path);
		return -1;
	}

	return 0;
}

static int _rmdir_recurs_foreach(void *opaque, git_buf *path)
{
	git_directory_removal_type removal_type = *(git_directory_removal_type *)opaque;

	assert(removal_type == GIT_DIRREMOVAL_EMPTY_HIERARCHY
		|| removal_type == GIT_DIRREMOVAL_FILES_AND_DIRS
		|| removal_type == GIT_DIRREMOVAL_ONLY_EMPTY_DIRS);

	if (git_path_isdir(path->ptr) == true) {
		if (git_path_direach(path, _rmdir_recurs_foreach, opaque) < 0)
			return -1;

		if (p_rmdir(path->ptr) < 0) {
			if (removal_type == GIT_DIRREMOVAL_ONLY_EMPTY_DIRS && (errno == ENOTEMPTY || errno == EEXIST))
				return 0;

			giterr_set(GITERR_OS, "Could not remove directory '%s'", path->ptr);
			return -1;
		}

		return 0;
	}

	if (removal_type == GIT_DIRREMOVAL_FILES_AND_DIRS) {
		if (p_unlink(path->ptr) < 0) {
			giterr_set(GITERR_OS, "Could not remove directory.  File '%s' cannot be removed", path->ptr);
			return -1;
		}

		return 0;
	}

	if (removal_type == GIT_DIRREMOVAL_EMPTY_HIERARCHY) {
		giterr_set(GITERR_OS, "Could not remove directory. File '%s' still present", path->ptr);
		return -1;
	}

	return 0;
}

int git_futils_rmdir_r(const char *path, git_directory_removal_type removal_type)
{
	int error;
	git_buf p = GIT_BUF_INIT;

	error = git_buf_sets(&p, path);
	if (!error)
		error = _rmdir_recurs_foreach(&removal_type, &p);
	git_buf_free(&p);
	return error;
}

#ifdef GIT_WIN32
struct win32_path {
	wchar_t path[MAX_PATH];
	DWORD len;
};

static int win32_expand_path(struct win32_path *s_root, const wchar_t *templ)
{
	s_root->len = ExpandEnvironmentStringsW(templ, s_root->path, MAX_PATH);
	return s_root->len ? 0 : -1;
}

static int win32_find_file(git_buf *path, const struct win32_path *root, const char *filename)
{
	int error = 0;
	size_t len;
	wchar_t *file_utf16 = NULL;
	char *file_utf8 = NULL;

	if (!root || !filename || (len = strlen(filename)) == 0)
		return GIT_ENOTFOUND;

	/* allocate space for wchar_t path to file */
	file_utf16 = git__calloc(root->len + len + 2, sizeof(wchar_t));
	GITERR_CHECK_ALLOC(file_utf16);

	/* append root + '\\' + filename as wchar_t */
	memcpy(file_utf16, root->path, root->len * sizeof(wchar_t));

	if (*filename == '/' || *filename == '\\')
		filename++;

	if (gitwin_append_utf16(file_utf16 + root->len - 1, filename, len + 1) !=
		(int)len + 1) {
		error = -1;
		goto cleanup;
	}

	/* check access */
	if (_waccess(file_utf16, F_OK) < 0) {
		error = GIT_ENOTFOUND;
		goto cleanup;
	}

	/* convert to utf8 */
	if ((file_utf8 = gitwin_from_utf16(file_utf16)) == NULL)
		error = -1;
	else {
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
#ifdef GIT_WIN32
	struct win32_path root;

	if (win32_expand_path(&root, L"%PROGRAMFILES%\\Git\\etc\\") < 0 ||
		root.path[0] == L'%') /* i.e. no expansion happened */
	{
		giterr_set(GITERR_OS, "Cannot locate the system's Program Files directory");
		return -1;
	}

	if (win32_find_file(path, &root, filename) < 0) {
		git_buf_clear(path);
		return GIT_ENOTFOUND;
	}

	return 0;

#else
	if (git_buf_joinpath(path, "/etc", filename) < 0)
		return -1;

	if (git_path_exists(path->ptr) == true)
		return 0;

	git_buf_clear(path);
	return GIT_ENOTFOUND;
#endif
}

int git_futils_find_global_file(git_buf *path, const char *filename)
{
#ifdef GIT_WIN32
	struct win32_path root;

	if (win32_expand_path(&root, L"%USERPROFILE%\\") < 0 ||
		root.path[0] == L'%') /* i.e. no expansion happened */
	{
		giterr_set(GITERR_OS, "Cannot locate the user's profile directory");
		return -1;
	}

	if (win32_find_file(path, &root, filename) < 0) {
		git_buf_clear(path);
		return GIT_ENOTFOUND;
	}

	return 0;
#else
	const char *home = getenv("HOME");

	if (home == NULL) {
		giterr_set(GITERR_OS, "Global file lookup failed. "
			"Cannot locate the user's home directory");
		return -1;
	}

	if (git_buf_joinpath(path, home, filename) < 0)
		return -1;

	if (git_path_exists(path->ptr) == false) {
		git_buf_clear(path);
		return GIT_ENOTFOUND;
	}

	return 0;
#endif
}
