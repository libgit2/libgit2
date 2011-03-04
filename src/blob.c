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

#include "git2/common.h"
#include "git2/object.h"
#include "git2/repository.h"

#include "common.h"
#include "blob.h"

const char *git_blob_rawcontent(git_blob *blob)
{
	assert(blob);
	
	if (blob->content.data != NULL)
		return blob->content.data;

	if (blob->object.in_memory)
		return NULL;

	if (!blob->object.source.open && git_object__source_open((git_object *)blob) < GIT_SUCCESS)
		return NULL;

	return blob->object.source.raw.data;
}

int git_blob_rawsize(git_blob *blob)
{
	assert(blob);

	if (blob->content.data != NULL)
		return blob->content.len;

	return blob->object.source.raw.len;
}

void git_blob__free(git_blob *blob)
{
	gitfo_free_buf(&blob->content);
	free(blob);
}

int git_blob__parse(git_blob *blob)
{
	assert(blob);
	return GIT_SUCCESS;
}

int git_blob__writeback(git_blob *blob, git_odb_source *src)
{
	assert(blob->object.modified);

	if (blob->content.data == NULL)
		return GIT_EMISSINGOBJDATA;

	return git__source_write(src, blob->content.data, blob->content.len);
}

int git_blob_set_rawcontent(git_blob *blob, const void *buffer, size_t len)
{
	assert(blob && buffer);

	blob->object.modified = 1;

	git_object__source_close((git_object *)blob);

	if (blob->content.data != NULL)
		gitfo_free_buf(&blob->content);

	blob->content.data = git__malloc(len);
	blob->content.len = len;

	if (blob->content.data == NULL)
		return GIT_ENOMEM;

	memcpy(blob->content.data, buffer, len);

	return GIT_SUCCESS;
}

int git_blob_set_rawcontent_fromfile(git_blob *blob, const char *filename)
{
	assert(blob && filename);
	blob->object.modified = 1;

	if (blob->content.data != NULL)
		gitfo_free_buf(&blob->content);

	return gitfo_read_file(&blob->content, filename);
}

int git_blob_writefile(git_oid *written_id, git_repository *repo, const char *path)
{
	int error;
	git_blob *blob;

	if (gitfo_exists(path) < 0)
		return GIT_ENOTFOUND;

	if ((error = git_blob_new(&blob, repo)) < GIT_SUCCESS)
		return error;

	if ((error = git_blob_set_rawcontent_fromfile(blob, path)) < GIT_SUCCESS)
		return error;

	if ((error = git_object_write((git_object *)blob)) < GIT_SUCCESS)
		return error;

	git_oid_cpy(written_id, git_object_id((git_object *)blob));

	/* FIXME: maybe we don't want to free this already?
	 * the user may want to access it again */
	GIT_OBJECT_DECREF(repo, blob);
	return GIT_SUCCESS;
}

