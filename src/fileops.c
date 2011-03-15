#include "common.h"
#include "fileops.h"
#include <ctype.h>

static int force_path(const char *to)
{
	const int mode = 0755; /* or 0777 ? */
	int error = GIT_SUCCESS;
	char target_folder_path[GIT_PATH_MAX];

	error = git__dirname_r(target_folder_path, sizeof(target_folder_path), to);
	if (error < GIT_SUCCESS)
		return error;

	/* Does the containing folder exist? */
	if (gitfo_isdir(target_folder_path)) {
		git__joinpath(target_folder_path, target_folder_path, ""); /* Ensure there's a trailing slash */

		/* Let's create the tree structure */
		error = gitfo_mkdir_recurs(target_folder_path, mode);
		if (error < GIT_SUCCESS)
			return error;
	}

	return GIT_SUCCESS;
}

int gitfo_open(const char *path, int flags)
{
	int fd = open(path, flags | O_BINARY);
	return fd >= 0 ? fd : GIT_EOSERR;
}

int gitfo_creat(const char *path, int mode)
{
	int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, mode);
	return fd >= 0 ? fd : GIT_EOSERR;
}

int gitfo_creat_force(const char *path, int mode)
{
	if (force_path(path) < GIT_SUCCESS)
		return GIT_EOSERR;

	return gitfo_creat(path, mode);
}

int gitfo_read(git_file fd, void *buf, size_t cnt)
{
	char *b = buf;
	while (cnt) {
		ssize_t r = read(fd, b, cnt);
		if (r < 0) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
			return GIT_EOSERR;
		}
		if (!r) {
			errno = EPIPE;
			return GIT_EOSERR;
		}
		cnt -= r;
		b += r;
	}
	return GIT_SUCCESS;
}

int gitfo_write(git_file fd, void *buf, size_t cnt)
{
	char *b = buf;
	while (cnt) {
		ssize_t r = write(fd, b, cnt);
		if (r < 0) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
			return GIT_EOSERR;
		}
		if (!r) {
			errno = EPIPE;
			return GIT_EOSERR;
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
		return GIT_ENOTFOUND;

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
		return GIT_ENOTFOUND;

	if (!S_ISDIR(st.st_mode))
		return GIT_ENOTFOUND;

	return GIT_SUCCESS;
}

int gitfo_exists(const char *path)
{
	return access(path, F_OK);
}

git_off_t gitfo_size(git_file fd)
{
	struct stat sb;
	if (gitfo_fstat(fd, &sb))
		return GIT_EOSERR;
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
		return GIT_ERROR;

	if (((size = gitfo_size(fd)) < 0) || !git__is_sizet(size+1)) {
		gitfo_close(fd);
		return GIT_ERROR;
	}
	len = (size_t) size;

	if ((buff = git__malloc(len + 1)) == NULL) {
		gitfo_close(fd);
		return GIT_ERROR;
	}

	if (gitfo_read(fd, buff, len) < 0) {
		gitfo_close(fd);
		free(buff);
		return GIT_ERROR;
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
#ifdef GIT_WIN32
	/*
	 * Win32 POSIX compilance my ass. If the destination
	 * file exists, the `rename` call fails. This is as
	 * close as it gets with the Win32 API.
	 */
	return MoveFileEx(from, to, MOVEFILE_REPLACE_EXISTING) ? GIT_SUCCESS : GIT_EOSERR;
#else
	/* Don't even try this on Win32 */
	if (!link(from, to)) {
		gitfo_unlink(from);
		return GIT_SUCCESS;
	}

	if (!rename(from, to))
		return GIT_SUCCESS;

	return GIT_EOSERR;
#endif
}

int gitfo_mv_force(const char *from, const char *to)
{
	if (force_path(to) < GIT_SUCCESS)
		return GIT_EOSERR;

	return gitfo_mv(from, to);
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

/* cached diskio */
struct gitfo_cache {
	git_file fd;
	size_t cache_size, pos;
	unsigned char *cache;
};

gitfo_cache *gitfo_enable_caching(git_file fd, size_t cache_size)
{
	gitfo_cache *ioc;

	ioc = git__malloc(sizeof(*ioc));
	if (!ioc)
		return NULL;

	ioc->fd = fd;
	ioc->pos = 0;
	ioc->cache_size = cache_size;
	ioc->cache = git__malloc(cache_size);
	if (!ioc->cache) {
		free(ioc);
		return NULL;
	}

	return ioc;
}

GIT_INLINE(void) gitfo_add_to_cache(gitfo_cache *ioc, void *buf, size_t len)
{
	memcpy(ioc->cache + ioc->pos, buf, len);
	ioc->pos += len;
}

int gitfo_flush_cached(gitfo_cache *ioc)
{
	int result = GIT_SUCCESS;

	if (ioc->pos) {
		result = gitfo_write(ioc->fd, ioc->cache, ioc->pos);
		ioc->pos = 0;
	}

	return result;
}

int gitfo_write_cached(gitfo_cache *ioc, void *buff, size_t len)
{
	unsigned char *buf = buff;

	for (;;) {
		size_t space_left = ioc->cache_size - ioc->pos;
		/* cache if it's small */
		if (space_left > len) {
			gitfo_add_to_cache(ioc, buf, len);
			return GIT_SUCCESS;
		}

		/* flush the cache if it doesn't fit */
		if (ioc->pos) {
			int rc;
			gitfo_add_to_cache(ioc, buf, space_left);
			rc = gitfo_flush_cached(ioc);
			if (rc < 0)
				return rc;

			len -= space_left;
			buf += space_left;
		}

		/* write too-large chunks immediately */
		if (len > ioc->cache_size)
			return gitfo_write(ioc->fd, buf, len);
	}
}

int gitfo_close_cached(gitfo_cache *ioc)
{
	git_file fd;

	if (gitfo_flush_cached(ioc) < GIT_SUCCESS)
		return GIT_ERROR;

	fd = ioc->fd;
	free(ioc->cache);
	free(ioc);

	return gitfo_close(fd);
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
		return GIT_ERROR;

	while (path[wd_len - 1] == '/')
		wd_len--;
	path[wd_len++] = '/';
	path[wd_len] = '\0';

	dir = opendir(path);
	if (!dir)
		return GIT_EOSERR;

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
			return GIT_ERROR;
		}

		strcpy(path + wd_len, de->d_name);
		result = fn(arg, path);
		if (result < GIT_SUCCESS) {
			closedir(dir);
			return result;
		}
		if (result > 0) {
			closedir(dir);
			return result;
		}
	}

	closedir(dir);
	return GIT_SUCCESS;
}

#ifdef GIT_WIN32

static int is_windows_rooted_path(const char *path)
{
	/* Does the root of the path look like a windows drive ? */
	if (isalpha(path[0]) && (path[1] == ':'))
		return GIT_SUCCESS;

	return GIT_ERROR;
}

#endif

int gitfo_mkdir_recurs(const char *path, int mode)
{
	int error;
	char *pp, *sp;
    char *path_copy = git__strdup(path);

	if (path_copy == NULL)
		return GIT_ENOMEM;

	error = GIT_SUCCESS;
	pp = path_copy;

#ifdef GIT_WIN32

	if (!is_windows_rooted_path(pp))
		pp += 2; /* Skip the drive name (eg. C: or D:) */

#endif

    while (error == GIT_SUCCESS && (sp = strchr(pp, '/')) != 0) {
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

	if (*(pp - 1) != '/' && error == GIT_SUCCESS)
		error = gitfo_mkdir(path, mode);

	free(path_copy);
	return error;
}

static int retrieve_previous_path_component_start(const char *path)
{
	int offset, len, start = 0;
	
	len = strlen(path);
	offset = len - 1;

	/* Skip leading slash */
	if (path[start] == '/')
		start++;

	/* Skip trailing slash */
	if (path[offset] == '/')
		offset--;

	if (offset < 0)
		return GIT_ERROR;

	while (offset > start && path[offset-1] != '/') {
		offset--;
	}

	return offset;
}

int gitfo_prettify_dir_path(char *buffer_out, const char *path)
{
	int len = 0, segment_len, only_dots;
	char *current;
	const char *buffer_out_start, *buffer_end;

	buffer_out_start = buffer_out;
	current = (char *)path;
	buffer_end = path + strlen(path);

	while (current < buffer_end) {
		/* Prevent multiple slashes from being added to the output */
		if (*current == '/' && len > 0 && buffer_out_start[len - 1] == '/') {
			current++;
			continue;
		}
		
		only_dots = 1;
		segment_len = 0;

		/* Copy path segment to the output */
		while (current < buffer_end && *current !='/')
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
			if (len < GIT_SUCCESS)
				return GIT_EINVALIDPATH;

			buffer_out = (char *)buffer_out_start + len;
			continue;
		}

		/* Guard against potential multiple dot path traversal (cf http://cwe.mitre.org/data/definitions/33.html) */
		if (only_dots &&segment_len > 0)
			return GIT_EINVALIDPATH;

		*buffer_out++ = '/';
		len++;
	}

	*buffer_out = '\0';

	return GIT_SUCCESS;
}

int gitfo_prettify_file_path(char *buffer_out, const char *path)
{
	int error, path_len, i;
	const char* pattern = "/..";

	path_len = strlen(path);

	/* Let's make sure the filename doesn't end with "/", "/." or "/.." */
	for (i = 1; path_len > i && i < 4; i++) {
		if (!strncmp(path + path_len - i, pattern, i))
			return GIT_EINVALIDPATH;
	}

	error =  gitfo_prettify_dir_path(buffer_out, path);
	if (error < GIT_SUCCESS)
		return error;

	path_len = strlen(buffer_out);
	if (path_len < 2)
		return GIT_EINVALIDPATH;

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

static void posixify_path(char *path)
{
	while (*path) {
		if (*path == '\\')
			*path = '/';

		path++;
	}
}

int gitfo_getcwd(char *buffer_out, size_t size)
{
	char *cwd_buffer;

	assert(buffer_out && size > 0);

#ifdef GIT_WIN32
	cwd_buffer = _getcwd(buffer_out, size);
#else
	cwd_buffer = getcwd(buffer_out, size); //TODO: Fixme. Ensure the required headers are correctly included
#endif

	if (cwd_buffer == NULL)
		return GIT_EOSERR;

	posixify_path(buffer_out);

	git__joinpath(buffer_out, buffer_out, "");	//Ensure the path ends with a trailing slash

	return GIT_SUCCESS;
}

