#include "common.h"
#include "fileops.h"

int gitfo_read(git_file fd, void *buf, size_t cnt)
{
	char *b = buf;
	while (cnt) {
		ssize_t r = read(fd, b, cnt);
		if (r < 0) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
			return -1;
		}
		if (!r) {
			errno = EPIPE;
			return -1;
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
			return -1;
		}
		if (!r) {
			errno = EPIPE;
			return -1;
		}
		cnt -= r;
		b += r;
	}
	return GIT_SUCCESS;
}

off_t gitfo_size(git_file fd)
{
	gitfo_statbuf sb;
	if (fstat(fd, &sb))
		return -1;
	return sb.st_size;
}

int gitfo_read_file(gitfo_buf *obj, const char *path)
{
	git_file fd;
	off_t len;
	unsigned char *buff;

	assert(obj && path && *path);

	if ((fd = gitfo_open(path, O_RDONLY)) < 0)
		return GIT_ERROR;  /* TODO: error handling */

	if (((len = gitfo_size(fd)) < 0) || ((buff = malloc(len+1)) == NULL)) {
		gitfo_close(fd);
		return GIT_ERROR;  /* TODO: error handling */
	}

	if (gitfo_read(fd, buff, len) < 0) {
		gitfo_close(fd);
		free(buff);
		return GIT_ERROR;  /* TODO: error handling */
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

/* cached diskio */
struct gitfo_cache {
	git_file fd;
	unsigned int cache_size, pos;
	void *cache;
};

gitfo_cache *gitfo_enable_caching(git_file fd, size_t cache_size)
{
	gitfo_cache *ioc;

	ioc = malloc(sizeof(*ioc));
	if (!ioc)
		return NULL;

	ioc->pos = 0;
	ioc->cache_size = cache_size;
	ioc->cache = malloc(cache_size);
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

int gitfo_write_cached(gitfo_cache *ioc, void *buf, size_t len)
{
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
	return GIT_SUCCESS;
}

int gitfo_close_cached(gitfo_cache *ioc)
{
	git_file fd;

	if (gitfo_flush_cached(ioc) < 0)
		return -1;

	fd = ioc->fd;
	free(ioc->cache);
	free(ioc);

	return gitfo_close(fd);
}

/**
 * Walk a directory and run 'fn' for each encountered entry
 * (except '.' and '..').
 */
int git_foreach_dirent(const char *wd, int (*fn)(void *, const char *), void *arg)
{
	char path[GIT_PATH_MAX];
	size_t wd_len;
	DIR *dir;
	struct dirent *de;

	if (!wd)
		return GIT_ERROR;

	wd_len = strlen(wd);
	if (!wd_len || sizeof(path) < wd_len + 2)
		return GIT_ERROR;

	strcpy(path, wd);
	while (path[wd_len - 1] == '/')
		wd_len--;
	path[wd_len++] = '/';
	path[wd_len] = '\0';

	dir = opendir(wd);
	if (!dir)
		return GIT_ERROR;

	while ((de = readdir(dir))) {
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
		if (sizeof(path) < wd_len + de_len + 1) {
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
