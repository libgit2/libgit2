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
	return git_futils_mkdir(
		file_path, NULL, mode, GIT_MKDIR_PATH | GIT_MKDIR_SKIP_LAST);
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
	wchar_t buf[GIT_WIN_PATH];

	git__utf8_to_16(buf, GIT_WIN_PATH, path);
	fd = _wopen(buf, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY | O_EXCL, mode);
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
		if (errno == ENOENT || errno == ENOTDIR)
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

int git_futils_mkdir(
	const char *path,
	const char *base,
	mode_t mode,
	uint32_t flags)
{
	git_buf make_path = GIT_BUF_INIT;
	ssize_t root = 0;
	char lastch, *tail;

	/* build path and find "root" where we should start calling mkdir */
	if (git_path_join_unrooted(&make_path, path, base, &root) < 0)
		return -1;

	if (make_path.size == 0) {
		giterr_set(GITERR_OS, "Attempt to create empty path");
		goto fail;
	}

	/* remove trailing slashes on path */
	while (make_path.ptr[make_path.size - 1] == '/') {
		make_path.size--;
		make_path.ptr[make_path.size] = '\0';
	}

	/* if we are not supposed to made the last element, truncate it */
	if ((flags & GIT_MKDIR_SKIP_LAST) != 0)
		git_buf_rtruncate_at_char(&make_path, '/');

	/* if we are not supposed to make the whole path, reset root */
	if ((flags & GIT_MKDIR_PATH) == 0)
		root = git_buf_rfind(&make_path, '/');

	/* clip root to make_path length */
	if (root >= (ssize_t)make_path.size)
		root = (ssize_t)make_path.size - 1;
	if (root < 0)
		root = 0;

	tail = & make_path.ptr[root];

	while (*tail) {
		/* advance tail to include next path component */
		while (*tail == '/')
			tail++;
		while (*tail && *tail != '/')
			tail++;

		/* truncate path at next component */
		lastch = *tail;
		*tail = '\0';

		/* make directory */
		if (p_mkdir(make_path.ptr, mode) < 0 &&
			(errno != EEXIST || (flags & GIT_MKDIR_EXCL) != 0))
		{
			giterr_set(GITERR_OS, "Failed to make directory '%s'",
				make_path.ptr);
			goto fail;
		}

		/* chmod if requested */
		if ((flags & GIT_MKDIR_CHMOD_PATH) != 0 ||
			((flags & GIT_MKDIR_CHMOD) != 0 && lastch == '\0'))
		{
			if (p_chmod(make_path.ptr, mode) < 0) {
				giterr_set(GITERR_OS, "Failed to set permissions on '%s'",
					make_path.ptr);
				goto fail;
			}
		}

		*tail = lastch;
	}

	git_buf_free(&make_path);
	return 0;

fail:
	git_buf_free(&make_path);
	return -1;
}

int git_futils_mkdir_r(const char *path, const char *base, const mode_t mode)
{
	return git_futils_mkdir(path, base, mode, GIT_MKDIR_PATH);
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
	size_t len, alloc_len;
	wchar_t *file_utf16 = NULL;
	char file_utf8[GIT_PATH_MAX];

	if (!root || !filename || (len = strlen(filename)) == 0)
		return GIT_ENOTFOUND;

	/* allocate space for wchar_t path to file */
	alloc_len = root->len + len + 2;
	file_utf16 = git__calloc(alloc_len, sizeof(wchar_t));
	GITERR_CHECK_ALLOC(file_utf16);

	/* append root + '\\' + filename as wchar_t */
	memcpy(file_utf16, root->path, root->len * sizeof(wchar_t));

	if (*filename == '/' || *filename == '\\')
		filename++;

	git__utf8_to_16(file_utf16 + root->len - 1, alloc_len, filename);

	/* check access */
	if (_waccess(file_utf16, F_OK) < 0) {
		git__free(file_utf16);
		return GIT_ENOTFOUND;
	}

	git__utf16_to_8(file_utf8, file_utf16);
	git_path_mkposix(file_utf8);
	git_buf_sets(path, file_utf8);

	git__free(file_utf16);
	return 0;
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
		giterr_set(GITERR_OS, "The system file '%s' doesn't exist", filename);
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
	giterr_set(GITERR_OS, "The system file '%s' doesn't exist", filename);
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
		giterr_set(GITERR_OS, "The global file '%s' doesn't exist", filename);
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
		giterr_set(GITERR_OS, "The global file '%s' doesn't exist", filename);
		git_buf_clear(path);
		return GIT_ENOTFOUND;
	}

	return 0;
#endif
}

int git_futils_fake_symlink(const char *old, const char *new)
{
	int retcode = GIT_ERROR;
	int fd = git_futils_creat_withpath(new, 0755, 0644);
	if (fd >= 0) {
		retcode = p_write(fd, old, strlen(old));
		p_close(fd);
	}
	return retcode;
}

static int cp_by_fd(int ifd, int ofd, bool close_fd_when_done)
{
	int error = 0;
	char buffer[4096];
	ssize_t len = 0;

	while (!error && (len = p_read(ifd, buffer, sizeof(buffer))) > 0)
		/* p_write() does not have the same semantics as write().  It loops
		 * internally and will return 0 when it has completed writing.
		 */
		error = p_write(ofd, buffer, len);

	if (len < 0) {
		giterr_set(GITERR_OS, "Read error while copying file");
		error = (int)len;
	}

	if (close_fd_when_done) {
		p_close(ifd);
		p_close(ofd);
	}

	return error;
}

int git_futils_cp(const char *from, const char *to, mode_t filemode)
{
	int ifd, ofd;

	if ((ifd = git_futils_open_ro(from)) < 0)
		return ifd;

	if ((ofd = p_open(to, O_WRONLY | O_CREAT | O_EXCL, filemode)) < 0) {
		if (errno == ENOENT || errno == ENOTDIR)
			ofd = GIT_ENOTFOUND;
		giterr_set(GITERR_OS, "Failed to open '%s' for writing", to);
		p_close(ifd);
		return ofd;
	}

	return cp_by_fd(ifd, ofd, true);
}

static int cp_link(const char *from, const char *to, size_t link_size)
{
	int error = 0;
	ssize_t read_len;
	char *link_data = git__malloc(link_size + 1);
	GITERR_CHECK_ALLOC(link_data);

	read_len = p_readlink(from, link_data, link_size);
	if (read_len != (ssize_t)link_size) {
		giterr_set(GITERR_OS, "Failed to read symlink data for '%s'", from);
		error = -1;
	}
	else {
		link_data[read_len] = '\0';

		if (p_symlink(link_data, to) < 0) {
			giterr_set(GITERR_OS, "Could not symlink '%s' as '%s'",
				link_data, to);
			error = -1;
		}
	}

	git__free(link_data);
	return error;
}

typedef struct {
	const char *to_root;
	git_buf to;
	ssize_t from_prefix;
	uint32_t flags;
	uint32_t mkdir_flags;
	mode_t dirmode;
} cp_r_info;

static int _cp_r_callback(void *ref, git_buf *from)
{
	cp_r_info *info = ref;
	struct stat from_st, to_st;
	bool exists = false;

	if ((info->flags & GIT_CPDIR_COPY_DOTFILES) == 0 &&
		from->ptr[git_path_basename_offset(from)] == '.')
		return 0;

	if (git_buf_joinpath(
			&info->to, info->to_root, from->ptr + info->from_prefix) < 0)
		return -1;

	if (p_lstat(info->to.ptr, &to_st) < 0) {
		if (errno != ENOENT && errno != ENOTDIR) {
			giterr_set(GITERR_OS,
				"Could not access %s while copying files", info->to.ptr);
			return -1;
		}
	} else
		exists = true;

	if (git_path_lstat(from->ptr, &from_st) < 0)
		return -1;

	if (S_ISDIR(from_st.st_mode)) {
		int error = 0;
		mode_t oldmode = info->dirmode;

		/* if we are not chmod'ing, then overwrite dirmode */
		if ((info->flags & GIT_CPDIR_CHMOD) == 0)
			info->dirmode = from_st.st_mode;

		/* make directory now if CREATE_EMPTY_DIRS is requested and needed */
		if (!exists && (info->flags & GIT_CPDIR_CREATE_EMPTY_DIRS) != 0)
			error = git_futils_mkdir(
				info->to.ptr, NULL, info->dirmode, info->mkdir_flags);

		/* recurse onto target directory */
		if (!exists || S_ISDIR(to_st.st_mode))
			error = git_path_direach(from, _cp_r_callback, info);

		if (oldmode != 0)
			info->dirmode = oldmode;

		return error;
	}

	if (exists) {
		if ((info->flags & GIT_CPDIR_OVERWRITE) == 0)
			return 0;

		if (p_unlink(info->to.ptr) < 0) {
			giterr_set(GITERR_OS, "Cannot overwrite existing file '%s'",
				info->to.ptr);
			return -1;
		}
	}

	/* Done if this isn't a regular file or a symlink */
	if (!S_ISREG(from_st.st_mode) &&
		(!S_ISLNK(from_st.st_mode) ||
		 (info->flags & GIT_CPDIR_COPY_SYMLINKS) == 0))
		return 0;

	/* Make container directory on demand if needed */
	if ((info->flags & GIT_CPDIR_CREATE_EMPTY_DIRS) == 0 &&
		git_futils_mkdir(
			info->to.ptr, NULL, info->dirmode, info->mkdir_flags) < 0)
		return -1;

	/* make symlink or regular file */
	if (S_ISLNK(from_st.st_mode))
		return cp_link(from->ptr, info->to.ptr, (size_t)from_st.st_size);
	else
		return git_futils_cp(from->ptr, info->to.ptr, from_st.st_mode);
}

int git_futils_cp_r(
	const char *from,
	const char *to,
	uint32_t flags,
	mode_t dirmode)
{
	int error;
	git_buf path = GIT_BUF_INIT;
	cp_r_info info;

	if (git_buf_sets(&path, from) < 0)
		return -1;

	info.to_root = to;
	info.flags   = flags;
	info.dirmode = dirmode;
	info.from_prefix = path.size;
	git_buf_init(&info.to, 0);

	/* precalculate mkdir flags */
	if ((flags & GIT_CPDIR_CREATE_EMPTY_DIRS) == 0) {
		info.mkdir_flags = GIT_MKDIR_PATH | GIT_MKDIR_SKIP_LAST;
		if ((flags & GIT_CPDIR_CHMOD) != 0)
			info.mkdir_flags |= GIT_MKDIR_CHMOD_PATH;
	} else {
		info.mkdir_flags =
			((flags & GIT_CPDIR_CHMOD) != 0) ? GIT_MKDIR_CHMOD : 0;
	}

	error = _cp_r_callback(&info, &path);

	git_buf_free(&path);
	git_buf_free(&info.to);

	return error;
}
