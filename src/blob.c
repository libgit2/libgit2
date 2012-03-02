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
	return GIT_SUCCESS;
}

int git_blob_create_frombuffer(git_oid *oid, git_repository *repo, const void *buffer, size_t len)
{
	int error;
	git_odb *odb;
	git_odb_stream *stream;

	error = git_repository_odb__weakptr(&odb, repo);
	if (error < GIT_SUCCESS)
		return error;

	if ((error = git_odb_open_wstream(&stream, odb, len, GIT_OBJ_BLOB)) < GIT_SUCCESS)
		return git__rethrow(error, "Failed to create blob");

	if ((error = stream->write(stream, buffer, len)) < GIT_SUCCESS) {
		stream->free(stream);
		return error;
	}

	error = stream->finalize_write(oid, stream);
	stream->free(stream);

	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to create blob");

	return GIT_SUCCESS;
}

static int write_file_stream(git_oid *oid, git_odb *odb, const char *path, git_off_t file_size)
{
	int fd, error;
	char buffer[4096];
	git_odb_stream *stream = NULL;

	if ((error = git_odb_open_wstream(&stream, odb, file_size, GIT_OBJ_BLOB)) < GIT_SUCCESS)
		return error;

	if ((fd = p_open(path, O_RDONLY)) < 0) {
		error = git__throw(GIT_ENOTFOUND, "Failed to create blob. Could not open '%s'", path);
		goto cleanup;
	}

	while (file_size > 0) {
		ssize_t read_len = p_read(fd, buffer, sizeof(buffer));

		if (read_len < 0) {
			error = git__throw(GIT_EOSERR, "Failed to create blob. Can't read full file");
			p_close(fd);
			goto cleanup;
		}

		stream->write(stream, buffer, read_len);
		file_size -= read_len;
	}

	p_close(fd);
	error = stream->finalize_write(oid, stream);

cleanup:
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

	error = git_futils_readbuffer(&source, full_path);
	if (error < GIT_SUCCESS)
		return error;

	error = git_filters_apply(&dest, &source, filters);

	/* Free the source as soon as possible. This can be big in memory,
	 * and we don't want to ODB write to choke */
	git_buf_free(&source);

	if (error == GIT_SUCCESS) {
		/* Write the file to disk if it was properly filtered */
		error = git_odb_write(oid, odb, dest.ptr, dest.size, GIT_OBJ_BLOB);
	}

	git_buf_free(&dest);
	return GIT_SUCCESS;
}

static int write_symlink(git_oid *oid, git_odb *odb, const char *path, size_t link_size)
{
	char *link_data;
	ssize_t read_len;
	int error;

	link_data = git__malloc(link_size);
	if (!link_data)
		return GIT_ENOMEM;

	read_len = p_readlink(path, link_data, link_size);

	if (read_len != (ssize_t)link_size) {
		free(link_data);
		return git__throw(GIT_EOSERR, "Failed to create blob. Can't read symlink");
	}

	error = git_odb_write(oid, odb, (void *)link_data, link_size, GIT_OBJ_BLOB);
	free(link_data);
	return error;
}

int git_blob_create_fromfile(git_oid *oid, git_repository *repo, const char *path)
{
	int error = GIT_SUCCESS;
	git_buf full_path = GIT_BUF_INIT;
	git_off_t size;
	struct stat st;
	const char *workdir;
	git_odb *odb = NULL;

	workdir = git_repository_workdir(repo);
	if (workdir == NULL)
		return git__throw(GIT_ENOTFOUND, "Failed to create blob. (No working directory found)");

	error = git_buf_joinpath(&full_path, workdir, path);
	if (error < GIT_SUCCESS)
		return error;

	error = p_lstat(full_path.ptr, &st);
	if (error < 0) {
		error = git__throw(GIT_EOSERR, "Failed to stat blob. %s", strerror(errno));
		goto cleanup;
	}

	size = st.st_size;

	error = git_repository_odb__weakptr(&odb, repo);
	if (error < GIT_SUCCESS)
		goto cleanup;

	if (S_ISLNK(st.st_mode)) {
		error = write_symlink(oid, odb, full_path.ptr, (size_t)size);
	} else {
		git_vector write_filters = GIT_VECTOR_INIT;
		int filter_count;

		/* Load the filters for writing this file to the ODB */
		filter_count = git_filters_load(&write_filters, repo, path, GIT_FILTER_TO_ODB);

		if (filter_count < 0) {
			/* Negative value means there was a critical error */
			error = filter_count;
			goto cleanup;
		} else if (filter_count == 0) {
			/* No filters need to be applied to the document: we can stream
			 * directly from disk */
			error = write_file_stream(oid, odb, full_path.ptr, size);
		} else {
			/* We need to apply one or more filters */
			error = write_file_filtered(oid, odb, full_path.ptr, &write_filters);
		}

		git_filters_free(&write_filters);

		/*
		 * TODO: eventually support streaming filtered files, for files which are bigger
		 * than a given threshold. This is not a priority because applying a filter in
		 * streaming mode changes the final size of the blob, and without knowing its
		 * final size, the blob cannot be written in stream mode to the ODB.
		 *
		 * The plan is to do streaming writes to a tempfile on disk and then opening
		 * streaming that file to the ODB, using `write_file_stream`.
		 *
		 * CAREFULLY DESIGNED APIS YO
		 */
	}

cleanup:
	git_buf_free(&full_path);
	return error;
}

