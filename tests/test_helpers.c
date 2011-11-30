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
#include "test_helpers.h"
#include "fileops.h"

int write_object_data(char *file, void *data, size_t len)
{
	git_file fd;
	int ret;

	if ((fd = p_creat(file, S_IREAD | S_IWRITE)) < 0)
		return -1;
	ret = p_write(fd, data, len);
	p_close(fd);

	return ret;
}

int write_object_files(const char *odb_dir, object_data *d)
{
	if (p_mkdir(odb_dir, GIT_OBJECT_DIR_MODE) < 0) {
		int err = errno;
		fprintf(stderr, "can't make directory \"%s\"", odb_dir);
		if (err == EEXIST)
			fprintf(stderr, " (already exists)");
		fprintf(stderr, "\n");
		return -1;
	}

	if ((p_mkdir(d->dir, GIT_OBJECT_DIR_MODE) < 0) && (errno != EEXIST)) {
		fprintf(stderr, "can't make object directory \"%s\"\n", d->dir);
		return -1;
	}
	if (write_object_data(d->file, d->bytes, d->blen) < 0) {
		fprintf(stderr, "can't write object file \"%s\"\n", d->file);
		return -1;
	}

	return 0;
}

int remove_object_files(const char *odb_dir, object_data *d)
{
	if (p_unlink(d->file) < 0) {
		fprintf(stderr, "can't delete object file \"%s\"\n", d->file);
		return -1;
	}
	if ((p_rmdir(d->dir) < 0) && (errno != ENOTEMPTY)) {
		fprintf(stderr, "can't remove object directory \"%s\"\n", d->dir);
		return -1;
	}

	if (p_rmdir(odb_dir) < 0) {
		fprintf(stderr, "can't remove directory \"%s\"\n", odb_dir);
		return -1;
	}

	return 0;
}

void locate_loose_object(const char *repository_folder, git_object *object, char **out, char **out_folder)
{
	static const char *objects_folder = "objects/";

	char *ptr, *full_path, *top_folder;
	int path_length, objects_length;

	assert(repository_folder && object);

	objects_length = strlen(objects_folder);
	path_length = strlen(repository_folder);
	ptr = full_path = git__malloc(path_length + objects_length + GIT_OID_HEXSZ + 3);

	strcpy(ptr, repository_folder);
	strcpy(ptr + path_length, objects_folder);

	ptr = top_folder = ptr + path_length + objects_length;
	*ptr++ = '/';
	git_oid_pathfmt(ptr, git_object_id(object));
	ptr += GIT_OID_HEXSZ + 1;
	*ptr = 0;

	*out = full_path;

	if (out_folder)
		*out_folder = top_folder;
}

int loose_object_mode(const char *repository_folder, git_object *object)
{
	char *object_path;
	struct stat st;

	locate_loose_object(repository_folder, object, &object_path, NULL);
	if (p_stat(object_path, &st) < 0)
		return 0;
	free(object_path);

	return st.st_mode;
}

int loose_object_dir_mode(const char *repository_folder, git_object *object)
{
	char *object_path;
	size_t pos;
	struct stat st;

	locate_loose_object(repository_folder, object, &object_path, NULL);

	pos = strlen(object_path);
	while (pos--) {
		if (object_path[pos] == '/') {
			object_path[pos] = 0;
			break;
		}
	}

	if (p_stat(object_path, &st) < 0)
		return 0;
	free(object_path);

	return st.st_mode;
}

int remove_loose_object(const char *repository_folder, git_object *object)
{
	char *full_path, *top_folder;

	locate_loose_object(repository_folder, object, &full_path, &top_folder);

	if (p_unlink(full_path) < 0) {
		fprintf(stderr, "can't delete object file \"%s\"\n", full_path);
		return -1;
	}

	*top_folder = 0;

	if ((p_rmdir(full_path) < 0) && (errno != ENOTEMPTY)) {
		fprintf(stderr, "can't remove object directory \"%s\"\n", full_path);
		return -1;
	}

	git__free(full_path);

	return GIT_SUCCESS;
}

int cmp_objects(git_rawobj *o, object_data *d)
{
	if (o->type != git_object_string2type(d->type))
		return -1;
	if (o->len != d->dlen)
		return -1;
	if ((o->len > 0) && (memcmp(o->data, d->data, o->len) != 0))
		return -1;
	return 0;
}

int copy_file(const char *src, const char *dst)
{
	git_fbuffer source_buf;
	git_file dst_fd;
	int error = GIT_ERROR;

	if (git_futils_readbuffer(&source_buf, src) < GIT_SUCCESS)
		return GIT_ENOTFOUND;

	dst_fd = git_futils_creat_withpath(dst, 0777, 0666);
	if (dst_fd < 0)
		goto cleanup;

	error = p_write(dst_fd, source_buf.data, source_buf.len);

cleanup:
	git_futils_freebuffer(&source_buf);
	p_close(dst_fd);

	return error;
}

int cmp_files(const char *a, const char *b)
{
	git_fbuffer buf_a, buf_b;
	int error = GIT_ERROR;

	if (git_futils_readbuffer(&buf_a, a) < GIT_SUCCESS)
		return GIT_ERROR;

	if (git_futils_readbuffer(&buf_b, b) < GIT_SUCCESS) {
		git_futils_freebuffer(&buf_a);
		return GIT_ERROR;
	}

	if (buf_a.len == buf_b.len && !memcmp(buf_a.data, buf_b.data, buf_a.len))
		error = GIT_SUCCESS;

	git_futils_freebuffer(&buf_a);
	git_futils_freebuffer(&buf_b);

	return error;
}

typedef struct {
	git_buf src;
	size_t  src_baselen;
	git_buf dst;
	size_t  dst_baselen;
} copydir_data;

static int copy_filesystem_element_recurs(void *_data, git_buf *source)
{
	copydir_data *data = (copydir_data *)_data;

	git_buf_truncate(&data->dst, data->dst_baselen);
	git_buf_joinpath(&data->dst, data->dst.ptr, source->ptr + data->src_baselen);

	if (git_futils_isdir(source->ptr) == GIT_SUCCESS)
		return git_futils_direach(source, copy_filesystem_element_recurs, _data);
	else
		return copy_file(source->ptr, data->dst.ptr);
}

int copydir_recurs(
	const char *source_directory_path,
	const char *destination_directory_path)
{
	int error;
	copydir_data data = { GIT_BUF_INIT, 0, GIT_BUF_INIT, 0 };

	/* Source has to exist, Destination hast to _not_ exist */
	if (git_futils_isdir(source_directory_path) != GIT_SUCCESS ||
		git_futils_isdir(destination_directory_path) == GIT_SUCCESS)
		return GIT_EINVALIDPATH;

	git_buf_joinpath(&data.src, source_directory_path, "");
	data.src_baselen = data.src.size;

	git_buf_joinpath(&data.dst, destination_directory_path, "");
	data.dst_baselen = data.dst.size;

	error = copy_filesystem_element_recurs(&data, &data.src);

	git_buf_free(&data.src);
	git_buf_free(&data.dst);

	return error;
}

int open_temp_repo(git_repository **repo, const char *path)
{
	int error;
	if ((error = copydir_recurs(path, TEMP_REPO_FOLDER)) < GIT_SUCCESS)
		return error;

	return git_repository_open(repo, TEMP_REPO_FOLDER);
}

void close_temp_repo(git_repository *repo)
{
	git_repository_free(repo);
	if (git_futils_rmdir_r(TEMP_REPO_FOLDER, 1) < GIT_SUCCESS) {
		printf("\nFailed to remove temporary folder. Aborting test suite.\n");
		exit(-1);
	}
}

typedef struct {
	const char *filename;
	size_t filename_len;
} remove_data;

static int remove_placeholders_recurs(void *_data, git_buf *path)
{
	remove_data *data = (remove_data *)_data;
	size_t pathlen;

	if (!git_futils_isdir(path->ptr))
		return git_futils_direach(path, remove_placeholders_recurs, data);

	pathlen = path->size;

	if (pathlen < data->filename_len)
		return GIT_SUCCESS;

	/* if path ends in '/'+filename (or equals filename) */
	if (!strcmp(data->filename, path->ptr + pathlen - data->filename_len) &&
		(pathlen == data->filename_len ||
		 path->ptr[pathlen - data->filename_len - 1] == '/'))
		return p_unlink(path->ptr);

	return GIT_SUCCESS;
}

int remove_placeholders(const char *directory_path, const char *filename)
{
	int error;
	remove_data data;
	git_buf buffer = GIT_BUF_INIT;

	if (git_futils_isdir(directory_path))
		return GIT_EINVALIDPATH;

	if ((error = git_buf_sets(&buffer, directory_path)) < GIT_SUCCESS)
		return error;

	data.filename = filename;
	data.filename_len = strlen(filename);

	error = remove_placeholders_recurs(&data, &buffer);

	git_buf_free(&buffer);

	return error;
}
