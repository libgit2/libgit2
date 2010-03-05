#include "common.h"
#include "fileops.h"

int gitfo_open(const char *path, int flags)
{
	int fd = open(path, flags | O_BINARY);
	return fd >= 0 ? fd : git_os_error();
}

int gitfo_creat(const char *path, int mode)
{
	int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, mode);
	return fd >= 0 ? fd : git_os_error();
}

int gitfo_read(git_file fd, void *buf, size_t cnt)
{
	char *b = buf;
	while (cnt) {
		ssize_t r = read(fd, b, cnt);
		if (r < 0) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
			return git_os_error();
		}
		if (!r) {
			errno = EPIPE;
			return git_os_error();
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
			return git_os_error();
		}
		if (!r) {
			errno = EPIPE;
			return git_os_error();
		}
		cnt -= r;
		b += r;
	}
	return GIT_SUCCESS;
}

int gitfo_exists(const char *path)
{
	return access(path, F_OK);
}

off_t gitfo_size(git_file fd)
{
	struct stat sb;
	if (gitfo_fstat(fd, &sb))
		return git_os_error();
	return sb.st_size;
}

int gitfo_read_file(gitfo_buf *obj, const char *path)
{
	git_file fd;
	size_t len;
	off_t size;
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

int gitfo_move_file(char *from, char *to)
{
	if (!link(from, to)) {
		gitfo_unlink(from);
		return GIT_SUCCESS;
	}

	if (!rename(from, to))
		return GIT_SUCCESS;

	return git_os_error();
}

int gitfo_map_ro(git_map *out, git_file fd, off_t begin, size_t len)
{
	if (git__mmap(out, len, GIT_PROT_READ, GIT_MAP_SHARED, fd, begin) < 0)
		return git_os_error();
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

	if (gitfo_flush_cached(ioc) < 0)
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
		return git_os_error();

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
		if (result < 0) {
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
