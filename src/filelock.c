/*
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2,
 * as published by the Free Software Foundation.
 *
 * In addition to the permissions in the GNU General Public License,
 * the authors give you unlimited permission to link the compiled
 * version of this file into combinations with other programs,
 * and to distribute those combinations without any restriction
 * coming from the use of this file.  (The General Public License
 * restrictions do apply in other respects; for example, they cover
 * modification of the file, and distribution when not linked into
 * a combined executable.)
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "common.h"
#include "filelock.h"
#include "fileops.h"

static const char *GIT_FILELOCK_EXTENSION = ".lock\0";
static const size_t GIT_FILELOCK_EXTLENGTH = 6;

#define BUILD_PATH_LOCK(_lock, _path) { \
	memcpy(_path, _lock->path, _lock->path_length); \
	memcpy(_path + _lock->path_length, GIT_FILELOCK_EXTENSION,\
			GIT_FILELOCK_EXTLENGTH);\
}

int git_filelock_init(git_filelock *lock, const char *path)
{
	if (lock == NULL || path == NULL)
		return GIT_ERROR;

	memset(lock, 0x0, sizeof(git_filelock));

	lock->path_length = strlen(path);

	if (lock->path_length + GIT_FILELOCK_EXTLENGTH >= GIT_PATH_MAX)
		return GIT_ERROR;

	memcpy(lock->path, path, lock->path_length);
	return 0;
}

int git_filelock_lock(git_filelock *lock, int append)
{
	char path_lock[GIT_PATH_MAX];
	BUILD_PATH_LOCK(lock, path_lock);

	/* If file already exists, we cannot create a lock */
	if (gitfo_exists(path_lock) == 0)
		return GIT_EOSERR;

	lock->file_lock = gitfo_creat(path_lock, 0666);

	if (lock->file_lock < 0)
		return GIT_EOSERR;

	lock->is_locked = 1;

	/* TODO: do a flock() in the descriptor file_lock */

	if (append && gitfo_exists(lock->path) == 0) {
		git_file source;
		char buffer[2048];
		size_t read_bytes;

		source = gitfo_open(lock->path, O_RDONLY);
		if (source < 0)
			return GIT_EOSERR;

		while ((read_bytes = gitfo_read(source, buffer, 2048)) > 0)
			gitfo_write(lock->file_lock, buffer, read_bytes);

		gitfo_close(source);
	}

	return 0;
}

void git_filelock_unlock(git_filelock *lock)
{
	char path_lock[GIT_PATH_MAX];
	BUILD_PATH_LOCK(lock, path_lock);

	if (lock->is_locked) {
		/* The flock() in lock->file_lock is removed
		 * automatically when closing the descriptor */
		gitfo_close(lock->file_lock);
		gitfo_unlink(path_lock);
		lock->is_locked = 0;
	}
}

int git_filelock_commit(git_filelock *lock)
{
	int error;
	char path_lock[GIT_PATH_MAX];
	BUILD_PATH_LOCK(lock, path_lock);

	if (!lock->is_locked || lock->file_lock < 0)
		return GIT_ERROR;

	/* FIXME: flush the descriptor? */
	gitfo_close(lock->file_lock);

	error = gitfo_move_file(path_lock, lock->path);

	if (error < 0)
		gitfo_unlink(path_lock);

	lock->is_locked = 0;
	return error;
}

int git_filelock_write(git_filelock *lock, const void *buffer, size_t length)
{
	if (!lock->is_locked || lock->file_lock < 0)
		return GIT_ERROR;

	return gitfo_write(lock->file_lock, (void *)buffer, length);
}
