#include "system.h"
#include "basics.h"
#include "reftable-error.h"

uint32_t reftable_rand(void)
{
	return rand();
}

int tmpfile_from_pattern(struct reftable_tmpfile *out, const char *pattern)
{
	char *path = reftable_strdup(pattern);
	int fd = mkstemp(path);
	if (fd < 0) {
		reftable_free(path);
		return REFTABLE_IO_ERROR;
	}

	out->path = path;
	out->fd = fd;

	return 0;
}

int tmpfile_close(struct reftable_tmpfile *t)
{
	int ret;

	if (t->fd < 0)
		return 0;
	ret = close(t->fd);
	t->fd = -1;

	if (ret < 0)
		return REFTABLE_IO_ERROR;
	return 0;
}

int tmpfile_delete(struct reftable_tmpfile *t)
{
	int ret;

	if (!t->path)
		return 0;

	tmpfile_close(t);
	if ((ret = unlink(t->path)) < 0)
		return REFTABLE_IO_ERROR;

	reftable_free((char *) t->path);
	t->path = NULL;

	return 0;
}

int tmpfile_rename(struct reftable_tmpfile *t, const char *path)
{
	int ret;

	if (!t->path)
		return REFTABLE_API_ERROR;

	tmpfile_close(t);

	if ((ret = rename(t->path, path)) < 0)
		return REFTABLE_IO_ERROR;

	reftable_free((char *) t->path);
	t->path = NULL;

	return 0;
}

struct libgit2_flock {
	char *lock_path;
	char *target_path;
};

int flock_acquire(struct reftable_flock *l, const char *target_path,
		  long timeout_ms)
{
	struct libgit2_flock *flock;
	struct timeval deadline, delta;
	unsigned multiplier = 1, n = 1;
	size_t lock_path_len;
	int fd = -1, error;

	lock_path_len = strlen(target_path) + strlen(".lock") + 1;
	if (!(flock = reftable_calloc(sizeof(*flock), 1)))
		goto out;
	if (!(flock->target_path = reftable_strdup(target_path)))
		goto out;
	if (!(flock->lock_path = reftable_malloc(lock_path_len)))
		goto out;
	snprintf(flock->lock_path, lock_path_len, "%s.lock", target_path);

	error = gettimeofday(&deadline, NULL);
	if (error < 0)
		goto out;

	delta.tv_sec = timeout_ms / 1000;
	delta.tv_usec = (timeout_ms % 1000) * 1000;

	timeradd(&delta, &deadline, &deadline);

	while (1) {
		struct timeval now;
		long wait_ms;

		if ((fd = open(flock->lock_path, O_WRONLY | O_EXCL | O_CREAT, 0666)) >= 0)
			break;

		error = gettimeofday(&now, NULL);
		if (error < 0)
			goto out;

		if (timercmp(&now, &deadline, >))
			goto out;

		wait_ms = (750 + rand() % 500) * multiplier / 1000;
		multiplier += 2 * n + 1;

		if (multiplier > 1000)
			multiplier = 1000;
		else
			n++;

		poll(NULL, 0, wait_ms);
	}

	l->priv = flock;
	l->path = flock->lock_path;
	l->fd = fd;

out:
	if (fd < 0) {
		if (flock) {
			reftable_free(flock->target_path);
			reftable_free(flock->lock_path);
		}

		reftable_free(flock);
		return REFTABLE_IO_ERROR;
	}
	return 0;
}

int flock_close(struct reftable_flock *l)
{
	int ret;

	if (l->fd < 0)
		return 0;

	ret = close(l->fd);
	l->fd = -1;

	if (ret < 0)
		return REFTABLE_IO_ERROR;
	return 0;
}

static void libgit2_flock_release(struct reftable_flock *l)
{
	struct libgit2_flock *flock = l->priv;
	reftable_free(flock->lock_path);
	reftable_free(flock->target_path);
	reftable_free(flock);
	l->priv = NULL;
	l->path = NULL;
}

int flock_release(struct reftable_flock *l)
{
	struct libgit2_flock *flock = l->priv;
	int ret;

	flock_close(l);

	if (!flock)
		return 0;

	if ((ret = unlink(flock->lock_path)) < 0)
		goto out;

out:
	libgit2_flock_release(l);
	if (ret < 0)
		return REFTABLE_IO_ERROR;
	return 0;
}

int flock_commit(struct reftable_flock *l)
{
	struct libgit2_flock *flock = l->priv;
	int ret;

	flock_close(l);

	if (!flock)
		return REFTABLE_API_ERROR;

	if ((ret = rename(flock->lock_path, flock->target_path)) < 0)
		goto out;

out:
	libgit2_flock_release(l);
	if (ret < 0)
		return REFTABLE_IO_ERROR;
	return 0;
}
