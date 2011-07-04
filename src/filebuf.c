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
	if (git_futils_exists(file->path_lock) == 0) {
		if (flags & GIT_FILEBUF_FORCE)
			p_unlink(file->path_lock);
		else
			return git__throw(GIT_EOSERR, "Failed to lock file");
	}

	/* create path to the file buffer is required */
	if (flags & GIT_FILEBUF_FORCE) {
		file->fd = git_futils_creat_locked_withpath(file->path_lock, 0644);
	} else {
		file->fd = git_futils_creat_locked(file->path_lock, 0644);
	}

	if (file->fd < 0)
		return git__throw(GIT_EOSERR, "Failed to create lock");

	if ((flags & GIT_FILEBUF_APPEND) && git_futils_exists(file->path_original) == 0) {
		git_file source;
		char buffer[2048];
		size_t read_bytes;

		source = p_open(file->path_original, O_RDONLY);
		if (source < 0)
			return git__throw(GIT_EOSERR, "Failed to lock file. Could not open %s", file->path_original);

		while ((read_bytes = p_read(source, buffer, 2048)) > 0) {
			p_write(file->fd, buffer, read_bytes);
			if (file->digest)
				git_hash_update(file->digest, buffer, read_bytes);
		}

		p_close(source);
	}

	return GIT_SUCCESS;
}

void git_filebuf_cleanup(git_filebuf *file)
{
	if (file->fd >= 0)
		p_close(file->fd);

	if (file->fd >= 0 && file->path_lock && git_futils_exists(file->path_lock) == GIT_SUCCESS)
		p_unlink(file->path_lock);

	if (file->digest)
		git_hash_free_ctx(file->digest);

	free(file->buffer);
	free(file->z_buf);

	deflateEnd(&file->zs);

	free(file->path_original);
	free(file->path_lock);
}

GIT_INLINE(int) flush_buffer(git_filebuf *file)
{
	int result = file->write(file, file->buffer, file->buf_pos);
	file->buf_pos = 0;
	return result;
}

static int write_normal(git_filebuf *file, const void *source, size_t len)
{
	int result = 0;

	if (len > 0) {
		result = p_write(file->fd, (void *)source, len);
		if (file->digest)
			git_hash_update(file->digest, source, len);
	}

	return result;
}

static int write_deflate(git_filebuf *file, const void *source, size_t len)
{
	int result = Z_OK;
	z_stream *zs = &file->zs;

	if (len > 0 || file->flush_mode == Z_FINISH) {
		zs->next_in = (void *)source;
		zs->avail_in = len;

		do {
			int have;

			zs->next_out = file->z_buf;
			zs->avail_out = file->buf_size;

            result = deflate(zs, file->flush_mode);
            assert(result != Z_STREAM_ERROR);

            have = file->buf_size - zs->avail_out;

			if (p_write(file->fd, file->z_buf, have) < GIT_SUCCESS)
				return git__throw(GIT_EOSERR, "Failed to write to file");

        } while (zs->avail_out == 0);

		assert(zs->avail_in == 0);

		if (file->digest)
			git_hash_update(file->digest, source, len);
	}

	return GIT_SUCCESS;
}

int git_filebuf_open(git_filebuf *file, const char *path, int flags)
{
	int error;
	size_t path_len;

	assert(file && path);

	memset(file, 0x0, sizeof(git_filebuf));

	file->buf_size = WRITE_BUFFER_SIZE;
	file->buf_pos = 0;
	file->fd = -1;

	/* Allocate the main cache buffer */
	file->buffer = git__malloc(file->buf_size);
	if (file->buffer == NULL){
		error = GIT_ENOMEM;
		goto cleanup;
	}

	/* If we are hashing on-write, allocate a new hash context */
	if (flags & GIT_FILEBUF_HASH_CONTENTS) {
		if ((file->digest = git_hash_new_ctx()) == NULL) {
			error = GIT_ENOMEM;
			goto cleanup;
		}
	}

	/* If we are deflating on-write, */
	if (flags & GIT_FILEBUF_DEFLATE_CONTENTS) {

		/* Initialize the ZLib stream */
		if (deflateInit(&file->zs, Z_BEST_SPEED) != Z_OK) {
			error = git__throw(GIT_EZLIB, "Failed to initialize zlib");
			goto cleanup;
		}

		/* Allocate the Zlib cache buffer */
		file->z_buf = git__malloc(file->buf_size);
		if (file->z_buf == NULL){
			error = GIT_ENOMEM;
			goto cleanup;
		}

		/* Never flush */
		file->flush_mode = Z_NO_FLUSH;
		file->write = &write_deflate;
	} else {
		file->write = &write_normal;
	}

	/* If we are writing to a temp file */
	if (flags & GIT_FILEBUF_TEMPORARY) {
		char tmp_path[GIT_PATH_MAX];

		/* Open the file as temporary for locking */
		file->fd = git_futils_mktmp(tmp_path, path);
		if (file->fd < 0) {
			error = GIT_EOSERR;
			goto cleanup;
		}

		/* No original path */
		file->path_original = NULL;
		file->path_lock = git__strdup(tmp_path);

		if (file->path_lock == NULL) {
			error = GIT_ENOMEM;
			goto cleanup;
		}
	} else {
		path_len = strlen(path);

		/* Save the original path of the file */
		file->path_original = git__strdup(path);
		if (file->path_original == NULL) {
			error = GIT_ENOMEM;
			goto cleanup;
		}

		/* create the locking path by appending ".lock" to the original */
		file->path_lock = git__malloc(path_len + GIT_FILELOCK_EXTLENGTH);
		if (file->path_lock == NULL) {
			error = GIT_ENOMEM;
			goto cleanup;
		}

		memcpy(file->path_lock, file->path_original, path_len);
		memcpy(file->path_lock + path_len, GIT_FILELOCK_EXTENSION, GIT_FILELOCK_EXTLENGTH);

		/* open the file for locking */
		if ((error = lock_file(file, flags)) < GIT_SUCCESS)
			goto cleanup;
	}

	return GIT_SUCCESS;

cleanup:
	git_filebuf_cleanup(file);
	return git__rethrow(error, "Failed to open file buffer for '%s'", path);
}

int git_filebuf_hash(git_oid *oid, git_filebuf *file)
{
	int error;

	assert(oid && file && file->digest);

	if ((error = flush_buffer(file)) < GIT_SUCCESS)
		return git__rethrow(error, "Failed to get hash for file");

	git_hash_final(oid, file->digest);
	git_hash_free_ctx(file->digest);
	file->digest = NULL;

	return GIT_SUCCESS;
}

int git_filebuf_commit_at(git_filebuf *file, const char *path)
{
	free(file->path_original);
	file->path_original = git__strdup(path);
	if (file->path_original == NULL)
		return GIT_ENOMEM;

	return git_filebuf_commit(file);
}

int git_filebuf_commit(git_filebuf *file)
{
	int error;

	/* temporary files cannot be committed */
	assert(file && file->path_original);

	file->flush_mode = Z_FINISH;
	if ((error = flush_buffer(file)) < GIT_SUCCESS)
		goto cleanup;

	p_close(file->fd);
	file->fd = -1;

	error = git_futils_mv_atomic(file->path_lock, file->path_original);

cleanup:
	git_filebuf_cleanup(file);
	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to commit locked file from buffer");
	return GIT_SUCCESS;
}

GIT_INLINE(void) add_to_cache(git_filebuf *file, const void *buf, size_t len)
{
	memcpy(file->buffer + file->buf_pos, buf, len);
	file->buf_pos += len;
}

int git_filebuf_write(git_filebuf *file, const void *buff, size_t len)
{
	int error;
	const unsigned char *buf = buff;

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
				return git__rethrow(error, "Failed to write to buffer");

			len -= space_left;
			buf += space_left;
		}

		/* write too-large chunks immediately */
		if (len > file->buf_size) {
			error = file->write(file, buf, len);
			if (error < GIT_SUCCESS)
				return git__rethrow(error, "Failed to write to buffer");
			return GIT_SUCCESS;
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
			return git__rethrow(error, "Failed to reserve buffer");
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
	va_end(arglist);

	if (len < 0 || (size_t)len >= space_left) {
		if ((error = flush_buffer(file)) < GIT_SUCCESS)
			return git__rethrow(error, "Failed to output to buffer");

		space_left = file->buf_size - file->buf_pos;

		va_start(arglist, format);
		len = vsnprintf((char *)file->buffer + file->buf_pos, space_left, format, arglist);
		va_end(arglist);

		if (len < 0 || (size_t)len > file->buf_size)
			return GIT_ENOMEM;
	}

	file->buf_pos += len;
	return GIT_SUCCESS;

}

