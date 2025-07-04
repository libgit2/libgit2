#include "system.h"
#include "basics.h"
#include "rand.h"
#include "reftable-error.h"

uint32_t reftable_rand(void)
{
	return rand();
}

int tmpfile_from_pattern(struct reftable_tmpfile *out, const char *pattern)
{
	git_str path = GIT_STR_INIT;
	unsigned tries = 32;

	if (git__suffixcmp(pattern, ".XXXXXX"))
		return REFTABLE_API_ERROR;

	while (tries--) {
		uint64_t rand = git_rand_next();
		int fd;

		git_str_sets(&path, pattern);
		git_str_shorten(&path, 6);
		git_str_encode_hexstr(&path, (void *)&rand, 6);

		if (git_str_oom(&path))
			return REFTABLE_OUT_OF_MEMORY_ERROR;

		if ((fd = p_open(path.ptr, O_CREAT | O_RDWR | O_EXCL | O_CLOEXEC, 0666)) < 0)
			continue;

		out->path = git_str_detach(&path);
		out->fd = fd;
		return 0;
	}

	git_str_dispose(&path);
	return REFTABLE_IO_ERROR;
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

	if ((ret = p_rename(t->path, path)) < 0)
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
	unsigned multiplier = 1, n = 1;
	size_t lock_path_len;
	uint64_t deadline;
	int fd = -1, error;

	lock_path_len = strlen(target_path) + strlen(".lock") + 1;
	if ((flock = reftable_calloc(sizeof(*flock), 1)) == NULL ||
	    (flock->target_path = reftable_strdup(target_path)) == NULL ||
	    (flock->lock_path = reftable_malloc(lock_path_len)) == NULL) {
		error = REFTABLE_OUT_OF_MEMORY_ERROR;
		goto out;
	}

	snprintf(flock->lock_path, lock_path_len, "%s.lock", target_path);

	deadline = reftable_time_ms() + timeout_ms;

	while (1) {
		uint64_t now, wait_ms;

		if ((fd = p_open(flock->lock_path, O_WRONLY | O_EXCL | O_CREAT, 0666)) >= 0)
			break;
		if (errno != EEXIST) {
			error = REFTABLE_IO_ERROR;
			goto out;
		}

		now = reftable_time_ms();
		if (now > deadline) {
			error = REFTABLE_LOCK_ERROR;
			goto out;
		}

		wait_ms = (750 + rand() % 500) * multiplier / 1000;
		multiplier += 2 * n + 1;

		if (multiplier > 1000)
			multiplier = 1000;
		else
			n++;

		reftable_sleep_ms(wait_ms);
	}

	l->priv = flock;
	l->path = flock->lock_path;
	l->fd = fd;

	error = 0;

out:
	if (error) {
		if (flock) {
			reftable_free(flock->target_path);
			reftable_free(flock->lock_path);
		}
		reftable_free(flock);
	}

	return error;
}

int flock_close(struct reftable_flock *l)
{
	int ret;

	if (l->fd < 0)
		return 0;

	ret = p_close(l->fd);
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

	if (!flock)
		return 0;

	flock_close(l);

	if ((ret = p_unlink(flock->lock_path)) < 0)
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

	if ((ret = p_rename(flock->lock_path, flock->target_path)) < 0)
		goto out;

out:
	libgit2_flock_release(l);
	if (ret < 0)
		return REFTABLE_IO_ERROR;
	return 0;
}
