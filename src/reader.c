/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "reader.h"

#include "fileops.h"
#include "blob.h"

#include "git2/tree.h"
#include "git2/blob.h"
#include "git2/index.h"
#include "git2/repository.h"

/* tree reader */

typedef struct {
	git_reader reader;
	git_tree *tree;
} tree_reader;

static int tree_reader_read(
	git_buf *out,
	git_oid *out_id,
	git_reader *_reader,
	const char *filename)
{
	tree_reader *reader = (tree_reader *)_reader;
	git_tree_entry *tree_entry = NULL;
	git_blob *blob = NULL;
	int error;

	if ((error = git_tree_entry_bypath(&tree_entry, reader->tree, filename)) < 0 ||
	    (error = git_blob_lookup(&blob, git_tree_owner(reader->tree), git_tree_entry_id(tree_entry))) < 0 ||
	    (error = git_buf_set(out, git_blob_rawcontent(blob), git_blob_rawsize(blob))) < 0)
		goto done;

	if (out_id)
		git_oid_cpy(out_id, git_tree_entry_id(tree_entry));

done:
	git_blob_free(blob);
	git_tree_entry_free(tree_entry);
	return error;
}

static void tree_reader_free(git_reader *_reader)
{
	GIT_UNUSED(_reader);
}

int git_reader_for_tree(git_reader **out, git_tree *tree)
{
	tree_reader *reader;

	assert(out && tree);

	reader = git__calloc(1, sizeof(tree_reader));
	GITERR_CHECK_ALLOC(reader);

	reader->reader.read = tree_reader_read;
	reader->reader.free = tree_reader_free;
	reader->tree = tree;

	*out = (git_reader *)reader;
	return 0;
}

/* workdir reader */

typedef struct {
	git_reader reader;
	git_repository *repo;
} workdir_reader;

static int workdir_reader_read(
	git_buf *out,
	git_oid *out_id,
	git_reader *_reader,
	const char *filename)
{
	workdir_reader *reader = (workdir_reader *)_reader;
	git_buf path = GIT_BUF_INIT;
	int error;

	if ((error = git_buf_joinpath(&path,
		git_repository_workdir(reader->repo), filename)) < 0)
		goto done;

	/* TODO: should we read the filtered data? */
	if ((error = git_futils_readbuffer(out, path.ptr)) < 0)
		goto done;

	if (out_id)
		error = git_odb_hash(out_id, out->ptr, out->size, GIT_OBJ_BLOB);

done:
	git_buf_dispose(&path);
	return error;
}

static void workdir_reader_free(git_reader *_reader)
{
	GIT_UNUSED(_reader);
}

int git_reader_for_workdir(git_reader **out, git_repository *repo)
{
	workdir_reader *reader;

	assert(out && repo);

	reader = git__calloc(1, sizeof(workdir_reader));
	GITERR_CHECK_ALLOC(reader);

	reader->reader.read = workdir_reader_read;
	reader->reader.free = workdir_reader_free;
	reader->repo = repo;

	*out = (git_reader *)reader;
	return 0;
}

/* index reader */

typedef struct {
	git_reader reader;
	git_repository *repo;
	git_index *index;
} index_reader;

static int index_reader_read(
	git_buf *out,
	git_oid *out_id,
	git_reader *_reader,
	const char *filename)
{
	index_reader *reader = (index_reader *)_reader;
	const git_index_entry *entry;
	git_blob *blob;
	int error;

	if ((entry = git_index_get_bypath(reader->index, filename, 0)) == NULL)
		return GIT_ENOTFOUND;

	if ((error = git_blob_lookup(&blob, reader->repo, &entry->id)) < 0)
		goto done;

	if (out_id)
		git_oid_cpy(out_id, &entry->id);

	error = git_blob__getbuf(out, blob);

done:
	git_blob_free(blob);
	return error;
}

static void index_reader_free(git_reader *_reader)
{
	GIT_UNUSED(_reader);
}

int git_reader_for_index(
	git_reader **out,
	git_repository *repo,
	git_index *index)
{
	index_reader *reader;
	int error;

	assert(out && repo);

	reader = git__calloc(1, sizeof(index_reader));
	GITERR_CHECK_ALLOC(reader);

	reader->reader.read = index_reader_read;
	reader->reader.free = index_reader_free;
	reader->repo = repo;

	if (index) {
		reader->index = index;
	} else {
		error = git_repository_index__weakptr(&reader->index, repo);

		if (error < 0) {
			git__free(reader);
			return error;
		}
	}

	*out = (git_reader *)reader;
	return 0;
}

/* generic */

int git_reader_read(
	git_buf *out,
	git_oid *out_id,
	git_reader *reader,
	const char *filename)
{
	assert(out && reader && filename);

	return reader->read(out, out_id, reader, filename);
}

void git_reader_free(git_reader *reader)
{
	if (!reader)
		return;

	reader->free(reader);
	git__free(reader);
}
