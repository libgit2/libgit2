/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "git2/common.h"
#include "git2/object.h"
#include "git2/repository.h"

#include "common.h"
#include "blob.h"
#include "filter.h"

const void *git_blob_rawcontent(git_blob *blob)
{
	assert(blob);
	return blob->odb_object->raw.data;
}

size_t git_blob_rawsize(git_blob *blob)
{
	assert(blob);
	return blob->odb_object->raw.len;
}

int git_blob__getbuf(git_buf *buffer, git_blob *blob)
{
	return git_buf_set(
		buffer, blob->odb_object->raw.data, blob->odb_object->raw.len);
}

void git_blob__free(git_blob *blob)
{
	git_odb_object_free(blob->odb_object);
	git__free(blob);
}

int git_blob__parse(git_blob *blob, git_odb_object *odb_obj)
{
	assert(blob);
	git_cached_obj_incref((git_cached_obj *)odb_obj);
	blob->odb_object = odb_obj;
	return 0;
}

int git_blob_create_frombuffer(git_oid *oid, git_repository *repo, const void *buffer, size_t len)
{
	int error;
	git_odb *odb;
	git_odb_stream *stream;

	if ((error = git_repository_odb__weakptr(&odb, repo)) < 0 ||
		(error = git_odb_open_wstream(&stream, odb, len, GIT_OBJ_BLOB)) < 0)
		return error;

	if ((error = stream->write(stream, buffer, len)) == 0)
		error = stream->finalize_write(oid, stream);

	stream->free(stream);
	return error;
}

static int write_file_stream(
	git_oid *oid, git_odb *odb, const char *path, git_off_t file_size)
{
	int fd, error;
	char buffer[4096];
	git_odb_stream *stream = NULL;
	ssize_t read_len = -1, written = 0;

	if ((error = git_odb_open_wstream(
			&stream, odb, (size_t)file_size, GIT_OBJ_BLOB)) < 0)
		return error;

	if ((fd = git_futils_open_ro(path)) < 0) {
		stream->free(stream);
		return -1;
	}

	while (!error && (read_len = p_read(fd, buffer, sizeof(buffer))) > 0) {
		error = stream->write(stream, buffer, read_len);
		written += read_len;
	}

	p_close(fd);

	if (written != file_size || read_len < 0) {
		giterr_set(GITERR_OS, "Failed to read file into stream");
		error = -1;
	}

	if (!error)
		error = stream->finalize_write(oid, stream);

	stream->free(stream);
	return error;
}

static int write_file_filtered(
	git_oid *oid,
	git_odb *odb,
	const char *full_path,
	git_vector *filters)
{
	int error;
	git_buf source = GIT_BUF_INIT;
	git_buf dest = GIT_BUF_INIT;

	if ((error = git_futils_readbuffer(&source, full_path)) < 0)
		return error;

	error = git_filters_apply(&dest, &source, filters);

	/* Free the source as soon as possible. This can be big in memory,
	 * and we don't want to ODB write to choke */
	git_buf_free(&source);

	/* Write the file to disk if it was properly filtered */
	if (!error)
		error = git_odb_write(oid, odb, dest.ptr, dest.size, GIT_OBJ_BLOB);

	git_buf_free(&dest);
	return error;
}

static int write_symlink(
	git_oid *oid, git_odb *odb, const char *path, size_t link_size)
{
	char *link_data;
	ssize_t read_len;
	int error;

	link_data = git__malloc(link_size);
	GITERR_CHECK_ALLOC(link_data);

	read_len = p_readlink(path, link_data, link_size);
	if (read_len != (ssize_t)link_size) {
		giterr_set(GITERR_OS, "Failed to create blob.  Can't read symlink '%s'", path);
		git__free(link_data);
		return -1;
	}

	error = git_odb_write(oid, odb, (void *)link_data, link_size, GIT_OBJ_BLOB);
	git__free(link_data);
	return error;
}

static int blob_create_internal(git_oid *oid, git_repository *repo, const char *content_path, const char *hint_path, bool try_load_filters)
{
	int error;
	struct stat st;
	git_odb *odb = NULL;
	git_off_t size;

	assert(hint_path || !try_load_filters);

	if ((error = git_path_lstat(content_path, &st)) < 0 || (error = git_repository_odb__weakptr(&odb, repo)) < 0)
		return error;

	size = st.st_size;

	if (S_ISLNK(st.st_mode)) {
		error = write_symlink(oid, odb, content_path, (size_t)size);
	} else {
		git_vector write_filters = GIT_VECTOR_INIT;
		int filter_count = 0;

		if (try_load_filters) {
			/* Load the filters for writing this file to the ODB */
			filter_count = git_filters_load(
				&write_filters, repo, hint_path, GIT_FILTER_TO_ODB);
		}

		if (filter_count < 0) {
			/* Negative value means there was a critical error */
			error = filter_count;
		} else if (filter_count == 0) {
			/* No filters need to be applied to the document: we can stream
			 * directly from disk */
			error = write_file_stream(oid, odb, content_path, size);
		} else {
			/* We need to apply one or more filters */
			error = write_file_filtered(oid, odb, content_path, &write_filters);
		}

		git_filters_free(&write_filters);

		/*
		 * TODO: eventually support streaming filtered files, for files
		 * which are bigger than a given threshold. This is not a priority
		 * because applying a filter in streaming mode changes the final
		 * size of the blob, and without knowing its final size, the blob
		 * cannot be written in stream mode to the ODB.
		 *
		 * The plan is to do streaming writes to a tempfile on disk and then
		 * opening streaming that file to the ODB, using
		 * `write_file_stream`.
		 *
		 * CAREFULLY DESIGNED APIS YO
		 */
	}

	return error;
}

int git_blob_create_fromfile(git_oid *oid, git_repository *repo, const char *path)
{
	git_buf full_path = GIT_BUF_INIT;
	const char *workdir;
	int error;

	if ((error = git_repository__ensure_not_bare(repo, "create blob from file")) < 0)
		return error;

	workdir = git_repository_workdir(repo);

	if (git_buf_joinpath(&full_path, workdir, path) < 0) {
		git_buf_free(&full_path);
		return -1;
	}

	error = blob_create_internal(oid, repo, git_buf_cstr(&full_path), git_buf_cstr(&full_path), true);

	git_buf_free(&full_path);
	return error;
}

int git_blob_create_fromdisk(git_oid *oid, git_repository *repo, const char *path)
{
	int error;
	git_buf full_path = GIT_BUF_INIT;

	if ((error = git_path_prettify(&full_path, path, NULL)) < 0) {
		git_buf_free(&full_path);
		return error;
	}

	error = blob_create_internal(oid, repo, git_buf_cstr(&full_path), git_buf_cstr(&full_path), true);

	git_buf_free(&full_path);
	return error;
}

#define BUFFER_SIZE 4096

int git_blob_create_fromchunks(
	git_oid *oid,
	git_repository *repo,
	const char *hintpath,
	int (*source_cb)(char *content, size_t max_length, void *payload),
	void *payload)
{
	int error = -1, read_bytes;
	char *content = NULL;
	git_filebuf file = GIT_FILEBUF_INIT;
	git_buf path = GIT_BUF_INIT;

	if (git_buf_join_n(
		&path, '/', 3, 
		git_repository_path(repo),
		GIT_OBJECTS_DIR, 
		"streamed") < 0)
			goto cleanup;

	content = git__malloc(BUFFER_SIZE);
	GITERR_CHECK_ALLOC(content);

	if (git_filebuf_open(&file, git_buf_cstr(&path), GIT_FILEBUF_TEMPORARY) < 0)
		goto cleanup;

	while (1) {
		read_bytes = source_cb(content, BUFFER_SIZE, payload);

		assert(read_bytes <= BUFFER_SIZE);

		if (read_bytes <= 0)
			break;

		if (git_filebuf_write(&file, content, read_bytes) < 0)
			goto cleanup;
	}

	if (read_bytes < 0)
		goto cleanup;

	if (git_filebuf_flush(&file) < 0)
		goto cleanup;

	error = blob_create_internal(oid, repo, file.path_lock, hintpath, hintpath != NULL);

cleanup:
	git_buf_free(&path);
	git_filebuf_cleanup(&file);
	git__free(content);
	return error;
}
