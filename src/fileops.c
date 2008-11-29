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

static inline void gitfo_add_to_cache(gitfo_cache *ioc, void *buf, size_t len)
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
