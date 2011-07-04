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
	if (p_mkdir(odb_dir, 0755) < 0) {
		int err = errno;
		fprintf(stderr, "can't make directory \"%s\"", odb_dir);
		if (err == EEXIST)
			fprintf(stderr, " (already exists)");
		fprintf(stderr, "\n");
		return -1;
	}

	if ((p_mkdir(d->dir, 0755) < 0) && (errno != EEXIST)) {
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

int remove_loose_object(const char *repository_folder, git_object *object)
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

	if (p_unlink(full_path) < 0) {
		fprintf(stderr, "can't delete object file \"%s\"\n", full_path);
		return -1;
	}

	*top_folder = 0;

	if ((p_rmdir(full_path) < 0) && (errno != ENOTEMPTY)) {
		fprintf(stderr, "can't remove object directory \"%s\"\n", full_path);
		return -1;
	}

	free(full_path);

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

	dst_fd = git_futils_creat_withpath(dst, 0644);
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

static int remove_filesystem_element_recurs(void *GIT_UNUSED(nil), char *path)
{
	int error = GIT_SUCCESS;

	GIT_UNUSED_ARG(nil);

	error = git_futils_isdir(path);
	if (error == GIT_SUCCESS) {
		size_t root_size = strlen(path);

		error = git_futils_direach(path, GIT_PATH_MAX, remove_filesystem_element_recurs, NULL);
		if (error < GIT_SUCCESS)
			return error;

		path[root_size] = 0;
		return rmdir(path);
	}

	return p_unlink(path);
}

int rmdir_recurs(const char *directory_path)
{
	char buffer[GIT_PATH_MAX];
	strcpy(buffer, directory_path);
	return remove_filesystem_element_recurs(NULL, buffer);
}

typedef struct {
	size_t src_len, dst_len;
	char *dst;
} copydir_data;

static int copy_filesystem_element_recurs(void *_data, char *source)
{
	copydir_data *data = (copydir_data *)_data;

	data->dst[data->dst_len] = 0;
	git_path_join(data->dst, data->dst, source + data->src_len);

	if (git_futils_isdir(source) == GIT_SUCCESS)
		return git_futils_direach(source, GIT_PATH_MAX, copy_filesystem_element_recurs, _data);

	return copy_file(source, data->dst);
}

int copydir_recurs(const char *source_directory_path, const char *destination_directory_path)
{
	char source_buffer[GIT_PATH_MAX];
	char dest_buffer[GIT_PATH_MAX];
	copydir_data data;

	/* Source has to exist, Destination hast to _not_ exist */
	if (git_futils_isdir(source_directory_path) != GIT_SUCCESS ||
		git_futils_isdir(destination_directory_path) == GIT_SUCCESS)
		return GIT_EINVALIDPATH;

	git_path_join(source_buffer, source_directory_path, "");
	data.src_len = strlen(source_buffer);

	git_path_join(dest_buffer, destination_directory_path, "");
	data.dst = dest_buffer;
	data.dst_len = strlen(dest_buffer);

	return copy_filesystem_element_recurs(&data, source_buffer);
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
	rmdir_recurs(TEMP_REPO_FOLDER);
}

static int remove_placeholders_recurs(void *filename, char *path)
{
	char passed_filename[GIT_PATH_MAX];
	char *data = (char *)filename;

	if (!git_futils_isdir(path))
		return git_futils_direach(path, GIT_PATH_MAX, remove_placeholders_recurs, data);

	if (git_path_basename_r(passed_filename, sizeof(passed_filename), path) < GIT_SUCCESS)
		return GIT_EINVALIDPATH;

	if (!strcmp(data, passed_filename))
		return p_unlink(path);

	return GIT_SUCCESS;
}

int remove_placeholders(char *directory_path, char *filename)
{
	char buffer[GIT_PATH_MAX];

	if (git_futils_isdir(directory_path))
		return GIT_EINVALIDPATH;

	strcpy(buffer, directory_path);
	return remove_placeholders_recurs(filename, buffer);
}
