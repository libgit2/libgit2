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
#include <stdarg.h>

#include "common.h"
#include "filebuf.h"
#include "fileops.h"

static const size_t WRITE_BUFFER_SIZE = (4096 * 2);

static int lock_file(git_filebuf *file, int flags)
{
	if (gitfo_exists(file->path_lock) == 0) {
		if (flags & GIT_FILEBUF_FORCE)
			gitfo_unlink(file->path_lock);
		else
			return GIT_EOSERR;
	}

	file->fd = gitfo_creat(file->path_lock, 0644);

	if (file->fd < 0)
		return GIT_EOSERR;

	/* TODO: do a flock() in the descriptor file_lock */

	if ((flags & GIT_FILEBUF_APPEND) && gitfo_exists(file->path_original) == 0) {
		git_file source;
		char buffer[2048];
		size_t read_bytes;

		source = gitfo_open(file->path_original, O_RDONLY);
		if (source < 0)
			return GIT_EOSERR;

		while ((read_bytes = gitfo_read(source, buffer, 2048)) > 0) {
			gitfo_write(file->fd, buffer, read_bytes);
			if (file->digest)
				git_hash_update(file->digest, buffer, read_bytes);
		}

		gitfo_close(source);
	}

	return GIT_SUCCESS;
}

void git_filebuf_cleanup(git_filebuf *file)
{
	if (file->fd >= 0)
		gitfo_close(file->fd);

	if (gitfo_exists(file->path_lock) == GIT_SUCCESS)
		gitfo_unlink(file->path_lock);

	if (file->digest)
		git_hash_free_ctx(file->digest);

	free(file->buffer);

#ifdef GIT_FILEBUF_THREADS
	free(file->buffer_back);
#endif

	free(file->path_original);
	free(file->path_lock);
}

static int flush_buffer(git_filebuf *file)
{
	int result = GIT_SUCCESS;

	if (file->buf_pos > 0) {
		result = gitfo_write(file->fd, file->buffer, file->buf_pos);
		if (file->digest)
			git_hash_update(file->digest, file->buffer, file->buf_pos);

		file->buf_pos = 0;
	}

	return result;
}

int git_filebuf_open(git_filebuf *file, const char *path, int flags)
{
	int error;
	size_t path_len;

	if (file == NULL || path == NULL)
		return GIT_ERROR;

	memset(file, 0x0, sizeof(git_filebuf));

	file->buf_size = WRITE_BUFFER_SIZE;
	file->buf_pos = 0;
	file->fd = -1;

	path_len = strlen(path);

	file->path_original = git__strdup(path);
	if (file->path_original == NULL) {
		error = GIT_ENOMEM;
		goto cleanup;
	}

	file->path_lock = git__malloc(path_len + GIT_FILELOCK_EXTLENGTH);
	if (file->path_lock == NULL) {
		error = GIT_ENOMEM;
		goto cleanup;
	}

	memcpy(file->path_lock, file->path_original, path_len);
	memcpy(file->path_lock + path_len, GIT_FILELOCK_EXTENSION, GIT_FILELOCK_EXTLENGTH);

	file->buffer = git__malloc(file->buf_size);
	if (file->buffer == NULL){
		error = GIT_ENOMEM;
		goto cleanup;
	}

#ifdef GIT_FILEBUF_THREADS
	file->buffer_back = git__malloc(file->buf_size);
	if (file->buffer_back == NULL){
		error = GIT_ENOMEM;
		goto cleanup;
	}
#endif

	if (flags & GIT_FILEBUF_HASH_CONTENTS) {
		if ((file->digest = git_hash_new_ctx()) == NULL) {
			error = GIT_ENOMEM;
			goto cleanup;
		}
	}

	if ((error = lock_file(file, flags)) < GIT_SUCCESS)
		goto cleanup;

	return GIT_SUCCESS;

cleanup:
	git_filebuf_cleanup(file);
	return error;
}

int git_filebuf_hash(git_oid *oid, git_filebuf *file)
{
	int error;

	if (file->digest == NULL)
		return GIT_ERROR;

	if ((error = flush_buffer(file)) < GIT_SUCCESS)
		return error;

	git_hash_final(oid, file->digest);
	git_hash_free_ctx(file->digest);
	file->digest = NULL;

	return GIT_SUCCESS;
}

int git_filebuf_commit(git_filebuf *file)
{
	int error;

	if ((error = flush_buffer(file)) < GIT_SUCCESS)
		goto cleanup;

	gitfo_close(file->fd);
	file->fd = -1;

	error = gitfo_move_file(file->path_lock, file->path_original);

cleanup:
	git_filebuf_cleanup(file);
	return error;
}

GIT_INLINE(void) add_to_cache(git_filebuf *file, void *buf, size_t len)
{
	memcpy(file->buffer + file->buf_pos, buf, len);
	file->buf_pos += len;
}

int git_filebuf_write(git_filebuf *file, void *buff, size_t len)
{
	int error;
	unsigned char *buf = buff;

	for (;;) {
		size_t space_left = file->buf_size - file->buf_pos;

		/* cache if it's small */
		if (space_left > len) {
			add_to_cache(file, buf, len);
			return GIT_SUCCESS;
		}

		/* flush the cache if it doesn't fit */
		if (file->buf_pos > 0) {
			add_to_cache(file, buf, space_left);

			if ((error = flush_buffer(file)) < GIT_SUCCESS)
				return error;

			len -= space_left;
			buf += space_left;
		}

		/* write too-large chunks immediately */
		if (len > file->buf_size) {
			error = gitfo_write(file->fd, buf, len);
			if (file->digest)
				git_hash_update(file->digest, buf, len);
		}
	}
}

int git_filebuf_reserve(git_filebuf *file, void **buffer, size_t len)
{
	int error;
	size_t space_left = file->buf_size - file->buf_pos;

	*buffer = NULL;

	if (len > file->buf_size)
		return GIT_ENOMEM;

	if (space_left <= len) {
		if ((error = flush_buffer(file)) < GIT_SUCCESS)
			return error;
	}

	*buffer = (file->buffer + file->buf_pos);
	file->buf_pos += len;

	return GIT_SUCCESS;
}

int git_filebuf_printf(git_filebuf *file, const char *format, ...)
{
	va_list arglist;
	size_t space_left = file->buf_size - file->buf_pos;
	int len, error;

	va_start(arglist, format);

	len = vsnprintf((char *)file->buffer + file->buf_pos, space_left, format, arglist);

	if (len < 0 || (size_t)len >= space_left) {
		if ((error = flush_buffer(file)) < GIT_SUCCESS)
			return error;

		len = vsnprintf((char *)file->buffer + file->buf_pos, space_left, format, arglist);
		if (len < 0 || (size_t)len > file->buf_size)
			return GIT_ENOMEM;
	}

	file->buf_pos += len;
	return GIT_SUCCESS;

}

