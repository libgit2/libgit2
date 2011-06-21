#include "common.h"
#include "fileops.h"
#include <ctype.h>

int gitfo_mkdir_2file(const char *file_path)
{
	const int mode = 0755; /* or 0777 ? */
	int error = GIT_SUCCESS;
	char target_folder_path[GIT_PATH_MAX];

	error = git__dirname_r(target_folder_path, sizeof(target_folder_path), file_path);
	if (error < GIT_SUCCESS)
		return git__throw(GIT_EINVALIDPATH, "Failed to recursively build `%s` tree structure. Unable to parse parent folder name", file_path);

	/* Does the containing folder exist? */
	if (gitfo_isdir(target_folder_path)) {
		git__joinpath(target_folder_path, target_folder_path, ""); /* Ensure there's a trailing slash */

		/* Let's create the tree structure */
		error = gitfo_mkdir_recurs(target_folder_path, mode);
		if (error < GIT_SUCCESS)
			return error;	/* The callee already takes care of setting the correct error message. */
	}

	return GIT_SUCCESS;
}

int gitfo_mktemp(char *path_out, const char *filename)
{
	int fd;

	strcpy(path_out, filename);
	strcat(path_out, "_git2_XXXXXX");

#if defined(_MSC_VER)
	/* FIXME: there may be race conditions when multi-threading
	 * with the library */
	if (_mktemp_s(path_out, GIT_PATH_MAX) != 0)
		return git__throw(GIT_EOSERR, "Failed to make temporary file %s", path_out);

	fd = gitfo_creat(path_out, 0744);
#else
	fd = mkstemp(path_out);
#endif

	return fd >= 0 ? fd : git__throw(GIT_EOSERR, "Failed to create temporary file %s", path_out);
}

int gitfo_open(const char *path, int flags)
{
	int fd = open(path, flags | O_BINARY);
	return fd >= 0 ? fd : git__throw(GIT_EOSERR, "Failed to open %s", path);
}

int gitfo_creat(const char *path, int mode)
{
	int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, mode);
	return fd >= 0 ? fd : git__throw(GIT_EOSERR, "Failed to create file. Could not open %s", path);
}

int gitfo_creat_force(const char *path, int mode)
{
	if (gitfo_mkdir_2file(path) < GIT_SUCCESS)
		return git__throw(GIT_EOSERR, "Failed to create file %s", path);

	return gitfo_creat(path, mode);
}

int gitfo_creat_locked(const char *path, int mode)
{
	int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY | O_EXCL, mode);
	return fd >= 0 ? fd : git__throw(GIT_EOSERR, "Failed to create locked file. Could not open %s", path);
}

int gitfo_creat_locked_force(const char *path, int mode)
{
	if (gitfo_mkdir_2file(path) < GIT_SUCCESS)
		return git__throw(GIT_EOSERR, "Failed to create locked file %s", path);

	return gitfo_creat_locked(path, mode);
}

int gitfo_read(git_file fd, void *buf, size_t cnt)
{
	char *b = buf;
	while (cnt) {
		ssize_t r = read(fd, b, cnt);
		if (r < 0) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
			return git__throw(GIT_EOSERR, "Failed to read from file");
		}
		if (!r)
			break;
		cnt -= r;
		b += r;
	}
	return (int)(b - (char *)buf);
}

int gitfo_write(git_file fd, void *buf, size_t cnt)
{
	char *b = buf;
	while (cnt) {
		ssize_t r = write(fd, b, cnt);
		if (r < 0) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
			return git__throw(GIT_EOSERR, "Failed to write to file");
		}
		if (!r) {
			errno = EPIPE;
			return git__throw(GIT_EOSERR, "Failed to write to file");
		}
		cnt -= r;
		b += r;
	}
	return GIT_SUCCESS;
}

int gitfo_isdir(const char *path)
{
	struct stat st;
	int len, stat_error;

	if (!path)
		return git__throw(GIT_ENOTFOUND, "No path given to gitfo_isdir");

	len = strlen(path);

	/* win32: stat path for folders cannot end in a slash */
	if (path[len - 1] == '/') {
		char *path_fixed = NULL;
		path_fixed = git__strdup(path);
		path_fixed[len - 1] = 0;
		stat_error = gitfo_stat(path_fixed, &st);
		free(path_fixed);
	} else {
		stat_error = gitfo_stat(path, &st);
	}

	if (stat_error < GIT_SUCCESS)
		return git__throw(GIT_ENOTFOUND, "%s does not exist", path);

	if (!S_ISDIR(st.st_mode))
		return git__throw(GIT_ENOTFOUND, "%s is not a directory", path);

	return GIT_SUCCESS;
}

int gitfo_isfile(const char *path)
{
	struct stat st;
	int stat_error;

	if (!path)
		return git__throw(GIT_ENOTFOUND, "No path given to gitfo_isfile");

	stat_error = gitfo_stat(path, &st);

	if (stat_error < GIT_SUCCESS)
		return git__throw(GIT_ENOTFOUND, "%s does not exist", path);

	if (!S_ISREG(st.st_mode))
		return git__throw(GIT_ENOTFOUND, "%s is not a file", path);

	return GIT_SUCCESS;
}

int gitfo_exists(const char *path)
{
	assert(path);
	return access(path, F_OK);
}

git_off_t gitfo_size(git_file fd)
{
	struct stat sb;
	if (gitfo_fstat(fd, &sb))
		return git__throw(GIT_EOSERR, "Failed to get size of file. File missing or corrupted");
	return sb.st_size;
}

int gitfo_read_file(gitfo_buf *obj, const char *path)
{
	git_file fd;
	size_t len;
	git_off_t size;
	unsigned char *buff;

	assert(obj && path && *path);

	if ((fd = gitfo_open(path, O_RDONLY)) < 0)
		return git__throw(GIT_ERROR, "Failed to open %s for reading", path);

	if (((size = gitfo_size(fd)) < 0) || !git__is_sizet(size+1)) {
		gitfo_close(fd);
		return git__throw(GIT_ERROR, "Failed to read file `%s`. An error occured while calculating its size", path);
	}
	len = (size_t) size;

	if ((buff = git__malloc(len + 1)) == NULL) {
		gitfo_close(fd);
		return GIT_ENOMEM;
	}

	if (gitfo_read(fd, buff, len) < 0) {
		gitfo_close(fd);
		free(buff);
		return git__throw(GIT_ERROR, "Failed to read file `%s`", path);
	}
	buff[len] = '\0';

	gitfo_close(fd);

	obj->data = buff;
	obj->len  = len;

	return GIT_SUCCESS;
}

void gitfo_free_buf(gitfo_buf *obj)
{
	assert(obj);
	free(obj->data);
	obj->data = NULL;
}

int gitfo_mv(const char *from, const char *to)
{
	int error;

#ifdef GIT_WIN32
	/*
	 * Win32 POSIX compilance my ass. If the destination
	 * file exists, the `rename` call fails. This is as
	 * close as it gets with the Win32 API.
	 */
	error = MoveFileEx(from, to, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED) ? GIT_SUCCESS : GIT_EOSERR;
#else
	/* Don't even try this on Win32 */
	if (!link(from, to)) {
		gitfo_unlink(from);
		return GIT_SUCCESS;
	}

	if (!rename(from, to))
		return GIT_SUCCESS;

	error = GIT_EOSERR;
#endif

	if (error < GIT_SUCCESS)
		return git__throw(error, "Failed to move file from `%s` to `%s`", from, to);

	return GIT_SUCCESS;
}

int gitfo_mv_force(const char *from, const char *to)
{
	if (gitfo_mkdir_2file(to) < GIT_SUCCESS)
		return GIT_EOSERR;	/* The callee already takes care of setting the correct error message. */

	return gitfo_mv(from, to);	/* The callee already takes care of setting the correct error message. */
}

int gitfo_map_ro(git_map *out, git_file fd, git_off_t begin, size_t len)
{
	if (git__mmap(out, len, GIT_PROT_READ, GIT_MAP_SHARED, fd, begin) < GIT_SUCCESS)
		return GIT_EOSERR;
	return GIT_SUCCESS;
}

void gitfo_free_map(git_map *out)
{
	git__munmap(out);
}

int gitfo_dirent(
	char *path,
	size_t path_sz,
	int (*fn)(void *, char *),
	void *arg)
{
	size_t wd_len = strlen(path);
	DIR *dir;
	struct dirent *de;

	if (!wd_len || path_sz < wd_len + 2)
		return git__throw(GIT_EINVALIDARGS, "Failed to process `%s` tree structure. Path is either empty or buffer size is too short", path);

	while (path[wd_len - 1] == '/')
		wd_len--;
	path[wd_len++] = '/';
	path[wd_len] = '\0';

	dir = opendir(path);
	if (!dir)
		return git__throw(GIT_EOSERR, "Failed to process `%s` tree structure. An error occured while opening the directory", path);

	while ((de = readdir(dir)) != NULL) {
		size_t de_len;
		int result;

		/* always skip '.' and '..' */
		if (de->d_name[0] == '.') {
			if (de->d_name[1] == '\0')
				continue;
			if (de->d_name[1] == '.' && de->d_name[2] == '\0')
				continue;
		}

		de_len = strlen(de->d_name);
		if (path_sz < wd_len + de_len + 1) {
			closedir(dir);
			return git__throw(GIT_ERROR, "Failed to process `%s` tree structure. Buffer size is too short", path);
		}

		strcpy(path + wd_len, de->d_name);
		result = fn(arg, path);
		if (result < GIT_SUCCESS) {
			closedir(dir);
			return result;	/* The callee is reponsible for setting the correct error message */
		}
		if (result > 0) {
			closedir(dir);
			return result;
		}
	}

	closedir(dir);
	return GIT_SUCCESS;
}

#if GIT_PLATFORM_PATH_SEP == '/'
void gitfo_posixify_path(char *GIT_UNUSED(path))
{
	/* nothing to do*/
}
#else
void gitfo_posixify_path(char *path)
{
	while (*path) {
		if (*path == GIT_PLATFORM_PATH_SEP)
			*path = '/';

		path++;
	}
}
#endif

int gitfo_retrieve_path_root_offset(const char *path)
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

int gitfo_mkdir_recurs(const char *path, int mode)
{
	int error, root_path_offset;
	char *pp, *sp;
    char *path_copy = git__strdup(path);

	if (path_copy == NULL)
		return GIT_ENOMEM;

	error = GIT_SUCCESS;
	pp = path_copy;

	root_path_offset = gitfo_retrieve_path_root_offset(pp);
	if (root_path_offset > 0)
		pp += root_path_offset; /* On Windows, will skip the drive name (eg. C: or D:) */

    while (error == GIT_SUCCESS && (sp = strchr(pp, '/')) != NULL) {
		if (sp != pp && gitfo_isdir(path_copy) < GIT_SUCCESS) {
			*sp = 0;
			error = gitfo_mkdir(path_copy, mode);

			/* Do not choke while trying to recreate an existing directory */
			if (errno == EEXIST)
				error = GIT_SUCCESS;

			*sp = '/';
		}

		pp = sp + 1;
	}

	if (*pp != '\0' && error == GIT_SUCCESS) {
		error = gitfo_mkdir(path, mode);
		if (errno == EEXIST)
			error = GIT_SUCCESS;
	}

	free(path_copy);

	if (error < GIT_SUCCESS)
		return git__throw(error, "Failed to recursively create `%s` tree structure", path);

	return GIT_SUCCESS;
}

static int retrieve_previous_path_component_start(const char *path)
{
	int offset, len, root_offset, start = 0;

	root_offset = gitfo_retrieve_path_root_offset(path);
	if (root_offset > -1)
		start += root_offset;

	len = strlen(path);
	offset = len - 1;

	/* Skip leading slash */
	if (path[start] == '/')
		start++;

	/* Skip trailing slash */
	if (path[offset] == '/')
		offset--;

	if (offset < root_offset)
		return git__throw(GIT_ERROR, "Failed to retrieve path component. Wrong offset");

	while (offset > start && path[offset-1] != '/') {
		offset--;
	}

	return offset;
}

int gitfo_prettify_dir_path(char *buffer_out, size_t size, const char *path, const char *base_path)
{
	int len = 0, segment_len, only_dots, root_path_offset, error = GIT_SUCCESS;
	char *current;
	const char *buffer_out_start, *buffer_end;

	current = (char *)path;
	buffer_end = path + strlen(path);
	buffer_out_start = buffer_out;

	root_path_offset = gitfo_retrieve_path_root_offset(path);
	if (root_path_offset < 0) {
		if (base_path == NULL) {
			error = gitfo_getcwd(buffer_out, size);
			if (error < GIT_SUCCESS)
				return error;	/* The callee already takes care of setting the correct error message. */
		} else {
			if (size < (strlen(base_path) + 1) * sizeof(char))
				return git__throw(GIT_EOVERFLOW, "Failed to prettify dir path: the base path is too long for the buffer.");

			strcpy(buffer_out, base_path);
			gitfo_posixify_path(buffer_out);
			git__joinpath(buffer_out, buffer_out, "");
		}

		len = strlen(buffer_out);
		buffer_out += len;
	}

	while (current < buffer_end) {
		/* Prevent multiple slashes from being added to the output */
		if (*current == '/' && len > 0 && buffer_out_start[len - 1] == '/') {
			current++;
			continue;
		}
		
		only_dots = 1;
		segment_len = 0;

		/* Copy path segment to the output */
		while (current < buffer_end && *current != '/')
		{
			only_dots &= (*current == '.');
			*buffer_out++ = *current++;
			segment_len++;
			len++;
		}

		/* Skip current directory */
		if (only_dots && segment_len == 1)
		{
			current++;
			buffer_out -= segment_len;
			len -= segment_len;
			continue;
		}

		/* Handle the double-dot upward directory navigation */
		if (only_dots && segment_len == 2)
		{
			current++;
			buffer_out -= segment_len;

			*buffer_out ='\0';
			len = retrieve_previous_path_component_start(buffer_out_start);

			/* Are we escaping out of the root dir? */
			if (len < 0)
				return git__throw(GIT_EINVALIDPATH, "Failed to normalize path `%s`. The path escapes out of the root directory", path);

			buffer_out = (char *)buffer_out_start + len;
			continue;
		}

		/* Guard against potential multiple dot path traversal (cf http://cwe.mitre.org/data/definitions/33.html) */
		if (only_dots && segment_len > 0)
			return git__throw(GIT_EINVALIDPATH, "Failed to normalize path `%s`. The path contains a segment with three `.` or more", path);

		*buffer_out++ = '/';
		len++;
	}

	*buffer_out = '\0';

	return GIT_SUCCESS;
}

int gitfo_prettify_file_path(char *buffer_out, size_t size, const char *path, const char *base_path)
{
	int error, path_len, i, root_offset;
	const char* pattern = "/..";

	path_len = strlen(path);

	/* Let's make sure the filename isn't empty nor a dot */
	if (path_len == 0 || (path_len == 1 && *path == '.'))
		return git__throw(GIT_EINVALIDPATH, "Failed to normalize file path `%s`. The path is either empty or equals `.`", path);

	/* Let's make sure the filename doesn't end with "/", "/." or "/.." */
	for (i = 1; path_len > i && i < 4; i++) {
		if (!strncmp(path + path_len - i, pattern, i))
			return git__throw(GIT_EINVALIDPATH, "Failed to normalize file path `%s`. The path points to a folder", path);
	}

	error =  gitfo_prettify_dir_path(buffer_out, size, path, base_path);
	if (error < GIT_SUCCESS)
		return error;	/* The callee already takes care of setting the correct error message. */

	path_len = strlen(buffer_out);
	root_offset = gitfo_retrieve_path_root_offset(buffer_out) + 1;
	if (path_len == root_offset)
		return git__throw(GIT_EINVALIDPATH, "Failed to normalize file path `%s`. The path points to a folder", path);

	/* Remove the trailing slash */
	buffer_out[path_len - 1] = '\0';

	return GIT_SUCCESS;
}

int gitfo_cmp_path(const char *name1, int len1, int isdir1,
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

int gitfo_getcwd(char *buffer_out, size_t size)
{
	char *cwd_buffer;

	assert(buffer_out && size > 0);

#ifdef GIT_WIN32
	cwd_buffer = _getcwd(buffer_out, size);
#else
	cwd_buffer = getcwd(buffer_out, size);
#endif

	if (cwd_buffer == NULL)
		return git__throw(GIT_EOSERR, "Failed to retrieve current working directory");

	gitfo_posixify_path(buffer_out);

	git__joinpath(buffer_out, buffer_out, "");	//Ensure the path ends with a trailing slash

	return GIT_SUCCESS;
}

#ifdef GIT_WIN32
GIT_INLINE(time_t) filetime_to_time_t(const FILETIME *ft)
{
	long long winTime = ((long long)ft->dwHighDateTime << 32) + ft->dwLowDateTime;
	winTime -= 116444736000000000LL; /* Windows to Unix Epoch conversion */
	winTime /= 10000000;		 /* Nano to seconds resolution */
	return (time_t)winTime;
}

static int do_lstat(const char *file_name, struct stat *buf)
{
	WIN32_FILE_ATTRIBUTE_DATA fdata;

	if (GetFileAttributesExA(file_name, GetFileExInfoStandard, &fdata)) {
		int fMode = S_IREAD;

		if (fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			fMode |= S_IFDIR;
		else
			fMode |= S_IFREG;

		if (!(fdata.dwFileAttributes & FILE_ATTRIBUTE_READONLY))
			fMode |= S_IWRITE;

		if (fdata.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
			fMode |= S_IFLNK;

		buf->st_ino = 0;
		buf->st_gid = 0;
		buf->st_uid = 0;
		buf->st_nlink = 1;
		buf->st_mode = (mode_t)fMode;
		buf->st_size = fdata.nFileSizeLow; /* Can't use nFileSizeHigh, since it's not a stat64 */
		buf->st_dev = buf->st_rdev = (_getdrive() - 1);
		buf->st_atime = filetime_to_time_t(&(fdata.ftLastAccessTime));
		buf->st_mtime = filetime_to_time_t(&(fdata.ftLastWriteTime));
		buf->st_ctime = filetime_to_time_t(&(fdata.ftCreationTime));
		return GIT_SUCCESS;
	}

	switch (GetLastError()) {
	case ERROR_ACCESS_DENIED:
	case ERROR_SHARING_VIOLATION:
	case ERROR_LOCK_VIOLATION:
	case ERROR_SHARING_BUFFER_EXCEEDED:
		return GIT_EOSERR;

	case ERROR_BUFFER_OVERFLOW:
	case ERROR_NOT_ENOUGH_MEMORY:
		return GIT_ENOMEM;

	default:
		return GIT_EINVALIDPATH;
	}
}

int gitfo_lstat__w32(const char *file_name, struct stat *buf)
{
	int namelen, error;
	char alt_name[GIT_PATH_MAX];

	if ((error = do_lstat(file_name, buf)) == GIT_SUCCESS)
		return GIT_SUCCESS;

	/* if file_name ended in a '/', Windows returned ENOENT;
	 * try again without trailing slashes
	 */
	if (error != GIT_EINVALIDPATH)
		return git__throw(GIT_EOSERR, "Failed to lstat file");

	namelen = strlen(file_name);
	if (namelen && file_name[namelen-1] != '/')
		return git__throw(GIT_EOSERR, "Failed to lstat file");

	while (namelen && file_name[namelen-1] == '/')
		--namelen;

	if (!namelen || namelen >= GIT_PATH_MAX)
		return git__throw(GIT_ENOMEM, "Failed to lstat file");

	memcpy(alt_name, file_name, namelen);
	alt_name[namelen] = 0;
	return do_lstat(alt_name, buf);
}

int gitfo_readlink__w32(const char *link, char *target, size_t target_len)
{
	typedef DWORD (WINAPI *fpath_func)(HANDLE, LPTSTR, DWORD, DWORD);
	static fpath_func pGetFinalPath = NULL;
	HANDLE hFile;
	DWORD dwRet;

	/*
	 * Try to load the pointer to pGetFinalPath dynamically, because
	 * it is not available in platforms older than Vista
	 */
	if (pGetFinalPath == NULL) {
		HINSTANCE library = LoadLibrary("kernel32");

		if (library != NULL)
			pGetFinalPath = (fpath_func)GetProcAddress(library, "GetFinalPathNameByHandleA");

		if (pGetFinalPath == NULL)
			return git__throw(GIT_EOSERR,
				"'GetFinalPathNameByHandleA' is not available in this platform");
	}

	hFile = CreateFile(link,            // file to open
				 GENERIC_READ,          // open for reading
				 FILE_SHARE_READ,       // share for reading
				 NULL,                  // default security
				 OPEN_EXISTING,         // existing file only
				 FILE_FLAG_BACKUP_SEMANTICS, // normal file
				 NULL);                 // no attr. template

	if (hFile == INVALID_HANDLE_VALUE)
		return GIT_EOSERR;

	dwRet = pGetFinalPath(hFile, target, target_len, 0x0);
	if (dwRet >= target_len)
		return GIT_ENOMEM;

	CloseHandle(hFile);

	if (dwRet > 4) {
		/* Skip first 4 characters if they are "\\?\" */
		if (target[0] == '\\' && target[1] == '\\' && target[2] == '?' && target[3] ==  '\\') {
			char tmp[GIT_PATH_MAX];
			unsigned int offset = 4;
			dwRet -= 4;

			/* \??\UNC\ */
			if (dwRet > 7 && target[4] == 'U' && target[5] == 'N' && target[6] == 'C') {
				offset += 2;
				dwRet -= 2;
				target[offset] = '\\';
			}

			memcpy(tmp, target + offset, dwRet);
			memcpy(target, tmp, dwRet);
		}
	}

	target[dwRet] = '\0';
	return dwRet;
}

#endif
