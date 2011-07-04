#include "common.h"
#include "fileops.h"
#include <ctype.h>

int git_futils_mv_atomic(const char *from, const char *to)
{
#ifdef GIT_WIN32
	/*
	 * Win32 POSIX compilance my ass. If the destination
	 * file exists, the `rename` call fails. This is as
	 * close as it gets with the Win32 API.
	 */
	return MoveFileEx(from, to, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED) ? GIT_SUCCESS : GIT_EOSERR;
#else
	/* Don't even try this on Win32 */
	if (!link(from, to)) {
		p_unlink(from);
		return GIT_SUCCESS;
	}

	if (!rename(from, to))
		return GIT_SUCCESS;

	return GIT_ERROR;
#endif
}

int git_futils_mkpath2file(const char *file_path)
{
	const int mode = 0755; /* or 0777 ? */
	int error = GIT_SUCCESS;
	char target_folder_path[GIT_PATH_MAX];

	error = git_path_dirname_r(target_folder_path, sizeof(target_folder_path), file_path);
	if (error < GIT_SUCCESS)
		return git__throw(GIT_EINVALIDPATH, "Failed to recursively build `%s` tree structure. Unable to parse parent folder name", file_path);

	/* Does the containing folder exist? */
	if (git_futils_isdir(target_folder_path)) {
		git_path_join(target_folder_path, target_folder_path, ""); /* Ensure there's a trailing slash */

		/* Let's create the tree structure */
		error = git_futils_mkdir_r(target_folder_path, mode);
		if (error < GIT_SUCCESS)
			return error;	/* The callee already takes care of setting the correct error message. */
	}

	return GIT_SUCCESS;
}

int git_futils_mktmp(char *path_out, const char *filename)
{
	int fd;

	strcpy(path_out, filename);
	strcat(path_out, "_git2_XXXXXX");

#if defined(_MSC_VER)
	/* FIXME: there may be race conditions when multi-threading
	 * with the library */
	if (_mktemp_s(path_out, GIT_PATH_MAX) != 0)
		return git__throw(GIT_EOSERR, "Failed to make temporary file %s", path_out);

	fd = p_creat(path_out, 0744);
#else
	fd = mkstemp(path_out);
#endif

	return fd >= 0 ? fd : git__throw(GIT_EOSERR, "Failed to create temporary file %s", path_out);
}

int git_futils_creat_withpath(const char *path, int mode)
{
	if (git_futils_mkpath2file(path) < GIT_SUCCESS)
		return git__throw(GIT_EOSERR, "Failed to create file %s", path);

	return p_creat(path, mode);
}

int git_futils_creat_locked(const char *path, int mode)
{
	int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY | O_EXCL, mode);
	return fd >= 0 ? fd : git__throw(GIT_EOSERR, "Failed to create locked file. Could not open %s", path);
}

int git_futils_creat_locked_withpath(const char *path, int mode)
{
	if (git_futils_mkpath2file(path) < GIT_SUCCESS)
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
	return access(path, F_OK);
}

git_off_t git_futils_filesize(git_file fd)
{
	struct stat sb;
	if (p_fstat(fd, &sb))
		return GIT_ERROR;

	return sb.st_size;
}

int git_futils_readbuffer(git_fbuffer *obj, const char *path)
{
	git_file fd;
	size_t len;
	git_off_t size;
	unsigned char *buff;

	assert(obj && path && *path);

	if ((fd = p_open(path, O_RDONLY)) < 0)
		return git__throw(GIT_ERROR, "Failed to open %s for reading", path);

	if (((size = git_futils_filesize(fd)) < 0) || !git__is_sizet(size+1)) {
		p_close(fd);
		return git__throw(GIT_ERROR, "Failed to read file `%s`. An error occured while calculating its size", path);
	}
	len = (size_t) size;

	if ((buff = git__malloc(len + 1)) == NULL) {
		p_close(fd);
		return GIT_ENOMEM;
	}

	if (p_read(fd, buff, len) < 0) {
		p_close(fd);
		free(buff);
		return git__throw(GIT_ERROR, "Failed to read file `%s`", path);
	}
	buff[len] = '\0';

	p_close(fd);

	obj->data = buff;
	obj->len  = len;

	return GIT_SUCCESS;
}

void git_futils_freebuffer(git_fbuffer *obj)
{
	assert(obj);
	free(obj->data);
	obj->data = NULL;
}


int git_futils_mv_withpath(const char *from, const char *to)
{
	if (git_futils_mkpath2file(to) < GIT_SUCCESS)
		return GIT_EOSERR;	/* The callee already takes care of setting the correct error message. */

	return git_futils_mv_atomic(from, to);	/* The callee already takes care of setting the correct error message. */
}

int git_futils_mmap_ro(git_map *out, git_file fd, git_off_t begin, size_t len)
{
	return p_mmap(out, len, GIT_PROT_READ, GIT_MAP_SHARED, fd, begin);
}

void git_futils_mmap_free(git_map *out)
{
	p_munmap(out);
}

int git_futils_direach(
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

int git_futils_mkdir_r(const char *path, int mode)
{
	int error, root_path_offset;
	char *pp, *sp;
    char *path_copy = git__strdup(path);

	if (path_copy == NULL)
		return GIT_ENOMEM;

	error = GIT_SUCCESS;
	pp = path_copy;

	root_path_offset = git_path_root(pp);
	if (root_path_offset > 0)
		pp += root_path_offset; /* On Windows, will skip the drive name (eg. C: or D:) */

    while (error == GIT_SUCCESS && (sp = strchr(pp, '/')) != NULL) {
		if (sp != pp && git_futils_isdir(path_copy) < GIT_SUCCESS) {
			*sp = 0;
			error = p_mkdir(path_copy, mode);

			/* Do not choke while trying to recreate an existing directory */
			if (errno == EEXIST)
				error = GIT_SUCCESS;

			*sp = '/';
		}

		pp = sp + 1;
	}

	if (*pp != '\0' && error == GIT_SUCCESS) {
		error = p_mkdir(path, mode);
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

	root_offset = git_path_root(path);
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

int git_futils_prettify_dir(char *buffer_out, size_t size, const char *path, const char *base_path)
{
	int len = 0, segment_len, only_dots, root_path_offset, error = GIT_SUCCESS;
	char *current;
	const char *buffer_out_start, *buffer_end;

	current = (char *)path;
	buffer_end = path + strlen(path);
	buffer_out_start = buffer_out;

	root_path_offset = git_path_root(path);
	if (root_path_offset < 0) {
		if (base_path == NULL) {
			error = p_getcwd(buffer_out, size);
			if (error < GIT_SUCCESS)
				return error;	/* The callee already takes care of setting the correct error message. */
		} else {
			if (size < (strlen(base_path) + 1) * sizeof(char))
				return git__throw(GIT_EOVERFLOW, "Failed to prettify dir path: the base path is too long for the buffer.");

			strcpy(buffer_out, base_path);
			git_path_mkposix(buffer_out);
			git_path_join(buffer_out, buffer_out, "");
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

int git_futils_prettyify_file(char *buffer_out, size_t size, const char *path, const char *base_path)
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

	error =  git_futils_prettify_dir(buffer_out, size, path, base_path);
	if (error < GIT_SUCCESS)
		return error;	/* The callee already takes care of setting the correct error message. */

	path_len = strlen(buffer_out);
	root_offset = git_path_root(buffer_out) + 1;
	if (path_len == root_offset)
		return git__throw(GIT_EINVALIDPATH, "Failed to normalize file path `%s`. The path points to a folder", path);

	/* Remove the trailing slash */
	buffer_out[path_len - 1] = '\0';

	return GIT_SUCCESS;
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

