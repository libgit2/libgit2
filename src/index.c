/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "repository.h"
#include "index.h"
#include "tree.h"
#include "hash.h"
#include "git2/odb.h"
#include "git2/oid.h"
#include "git2/blob.h"
#include "git2/config.h"

#define INDEX_VERSION_DEFAULT INDEX_VERSION_NUMBER

#define INDEX_OWNER(idx) ((git_repository *)(GIT_REFCOUNT_OWNER(idx)))

struct index_header {
	uint32_t signature;
	uint32_t version;
};

/* local declarations */
static void set_index_operations(git_index *index);
static int parse_index(git_index *index, const char *buffer, size_t buffer_size);

static void index_entry_free(git_index_entry *entry);

static int index_srch(const void *key, const void *array_member)
{
	const git_index_entry *entry = array_member;

	return strcmp(key, entry->path);
}

static int index_cmp(const void *a, const void *b)
{
	const git_index_entry *entry_a = a;
	const git_index_entry *entry_b = b;

	return strcmp(entry_a->path, entry_b->path);
}

static int unmerged_srch(const void *key, const void *array_member)
{
	const git_index_entry_unmerged *entry = array_member;

	return strcmp(key, entry->path);
}

static unsigned int index_create_mode(unsigned int mode)
{
	if (S_ISLNK(mode))
		return S_IFLNK;

	if (S_ISDIR(mode) || (mode & S_IFMT) == (S_IFLNK | S_IFDIR))
		return (S_IFLNK | S_IFDIR);

	return S_IFREG | ((mode & 0100) ? 0755 : 0644);
}

static unsigned int index_merge_mode(
	git_index *index, git_index_entry *existing, unsigned int mode)
{
	if (index->no_symlinks && S_ISREG(mode) &&
		existing && S_ISLNK(existing->mode))
		return existing->mode;

	if (index->distrust_filemode && S_ISREG(mode))
		return (existing && S_ISREG(existing->mode)) ?
			existing->mode : index_create_mode(0666);

	return index_create_mode(mode);
}

int git_index_open(git_index **index_out, const char *index_path)
{
	git_index *index;

	assert(index_out && index_path);

	index = git__calloc(1, sizeof(git_index));
	GITERR_CHECK_ALLOC(index);

	index->index_file_path = git__strdup(index_path);
	GITERR_CHECK_ALLOC(index->index_file_path);

	if (git_vector_init(&index->entries, 32, index_cmp) < 0)
		return -1;

	/* Check if index file is stored on disk already */
	if (git_path_exists(index->index_file_path) == true)
		index->on_disk = 1;

	*index_out = index;
	GIT_REFCOUNT_INC(index);
	return git_index_read(index);
}

static void index_free(git_index *index)
{
	git_index_entry *e;
	unsigned int i;

	git_index_clear(index);
	git_vector_foreach(&index->entries, i, e) {
		index_entry_free(e);
	}
	git_vector_free(&index->entries);
	git_vector_foreach(&index->unmerged, i, e) {
		index_entry_free(e);
	}
	git_vector_free(&index->unmerged);

	git__free(index->index_file_path);
	git__free(index);
}

void git_index_free(git_index *index)
{
	if (index == NULL)
		return;

	GIT_REFCOUNT_DEC(index, index_free);
}

void git_index_clear(git_index *index)
{
	unsigned int i;

	assert(index);

	for (i = 0; i < index->entries.length; ++i) {
		git_index_entry *e;
		e = git_vector_get(&index->entries, i);
		git__free(e->path);
		git__free(e);
	}

	for (i = 0; i < index->unmerged.length; ++i) {
		git_index_entry_unmerged *e;
		e = git_vector_get(&index->unmerged, i);
		git__free(e->path);
		git__free(e);
	}

	git_vector_clear(&index->entries);
	git_vector_clear(&index->unmerged);
	index->last_modified = 0;

	git_tree_cache_free(index->tree);
	index->tree = NULL;
}

int git_index_set_caps(git_index *index, unsigned int caps)
{
	assert(index);

	if (caps == GIT_INDEXCAP_FROM_OWNER) {
		git_config *cfg;
		int val;

		if (INDEX_OWNER(index) == NULL ||
			git_repository_config__weakptr(&cfg, INDEX_OWNER(index)) < 0)
		{
			giterr_set(GITERR_INDEX,
				"Cannot get repository config to set index caps");
			return -1;
		}

		if (git_config_get_bool(&val, cfg, "core.ignorecase") == 0)
			index->ignore_case = (val != 0);
		if (git_config_get_bool(&val, cfg, "core.filemode") == 0)
			index->distrust_filemode = (val == 0);
		if (git_config_get_bool(&val, cfg, "core.symlinks") == 0)
			index->no_symlinks = (val == 0);
	}
	else {
		index->ignore_case = ((caps & GIT_INDEXCAP_IGNORE_CASE) != 0);
		index->distrust_filemode = ((caps & GIT_INDEXCAP_NO_FILEMODE) != 0);
		index->no_symlinks = ((caps & GIT_INDEXCAP_NO_SYMLINKS) != 0);
	}

	return 0;
}

unsigned int git_index_caps(const git_index *index)
{
	return ((index->ignore_case ? GIT_INDEXCAP_IGNORE_CASE : 0) |
			(index->distrust_filemode ? GIT_INDEXCAP_NO_FILEMODE : 0) |
			(index->no_symlinks ? GIT_INDEXCAP_NO_SYMLINKS : 0));
}

int git_index_read(git_index *index)
{
	int error, updated;
	git_buf buffer = GIT_BUF_INIT;
	time_t mtime;

	assert(index->index_file_path);

	if (!index->on_disk || git_path_exists(index->index_file_path) == false) {
		git_index_clear(index);
		index->on_disk = 0;
		return 0;
	}

	/* We don't want to update the mtime if we fail to parse the index */
	mtime = index->last_modified;
	error = git_futils_readbuffer_updated(
		&buffer, index->index_file_path, &mtime, &updated);
	if (error < 0)
		return error;

	if (updated) {
		git_index_clear(index);

		error = parse_index(index, buffer.ptr, buffer.size);

		if (!error)
			index->last_modified = mtime;

		git_buf_free(&buffer);
	}

	return error;
}

int git_index_write(git_index *index)
{
	git_filebuf file = GIT_FILEBUF_INIT;
	struct stat indexst;
	int error;

	if ((error = git_filebuf_open(
			 &file, index->index_file_path, GIT_FILEBUF_HASH_CONTENTS)) < 0)
		return error;

	set_index_operations(index);

	if (index->operations == NULL)
		return index_error_invalid("incorrect header version");

	if ((error = index->operations->write(index, &file)) < 0) {
		git_filebuf_cleanup(&file);
		return error;
	}

	if ((error = git_filebuf_commit(&file, GIT_INDEX_FILE_MODE)) < 0)
		return error;

	if (p_stat(index->index_file_path, &indexst) == 0) {
		index->last_modified = indexst.st_mtime;
		index->on_disk = 1;
	}

	return 0;
}

unsigned int git_index_entrycount(git_index *index)
{
	assert(index);
	return (unsigned int)index->entries.length;
}

unsigned int git_index_entrycount_unmerged(git_index *index)
{
	assert(index);
	return (unsigned int)index->unmerged.length;
}

git_index_entry *git_index_get(git_index *index, size_t n)
{
	git_vector_sort(&index->entries);
	return git_vector_get(&index->entries, n);
}

void git_index__init_entry_from_stat(struct stat *st, git_index_entry *entry)
{
	entry->ctime.seconds = (git_time_t)st->st_ctime;
	entry->mtime.seconds = (git_time_t)st->st_mtime;
	/* entry->mtime.nanoseconds = st->st_mtimensec; */
	/* entry->ctime.nanoseconds = st->st_ctimensec; */
	entry->dev  = st->st_rdev;
	entry->ino  = st->st_ino;
	entry->mode = index_create_mode(st->st_mode);
	entry->uid  = st->st_uid;
	entry->gid  = st->st_gid;
	entry->file_size = st->st_size;
}

static int index_entry_init(git_index_entry **entry_out, git_index *index, const char *rel_path, int stage)
{
	git_index_entry *entry = NULL;
	struct stat st;
	git_oid oid;
	const char *workdir;
	git_buf full_path = GIT_BUF_INIT;
	int error;

	assert(stage >= 0 && stage <= 3);

	if (INDEX_OWNER(index) == NULL ||
		(workdir = git_repository_workdir(INDEX_OWNER(index))) == NULL)
	{
		giterr_set(GITERR_INDEX,
			"Could not initialize index entry. Repository is bare");
		return -1;
	}

	if ((error = git_buf_joinpath(&full_path, workdir, rel_path)) < 0)
		return error;

	if ((error = git_path_lstat(full_path.ptr, &st)) < 0) {
		git_buf_free(&full_path);
		return error;
	}

	git_buf_free(&full_path); /* done with full path */

	/* There is no need to validate the rel_path here, since it will be
	 * immediately validated by the call to git_blob_create_fromfile.
	 */

	/* write the blob to disk and get the oid */
	if ((error = git_blob_create_fromfile(&oid, INDEX_OWNER(index), rel_path)) < 0)
		return error;

	entry = git__calloc(1, sizeof(git_index_entry));
	GITERR_CHECK_ALLOC(entry);

	git_index__init_entry_from_stat(&st, entry);

	entry->oid = oid;
	entry->flags |= (stage << GIT_IDXENTRY_STAGESHIFT);
	entry->path = git__strdup(rel_path);
	GITERR_CHECK_ALLOC(entry->path);

	*entry_out = entry;
	return 0;
}

static git_index_entry *index_entry_dup(const git_index_entry *source_entry)
{
	git_index_entry *entry;

	entry = git__malloc(sizeof(git_index_entry));
	if (!entry)
		return NULL;

	memcpy(entry, source_entry, sizeof(git_index_entry));

	/* duplicate the path string so we own it */
	entry->path = git__strdup(entry->path);
	if (!entry->path)
		return NULL;

	return entry;
}

static void index_entry_free(git_index_entry *entry)
{
	if (!entry)
		return;
	git__free(entry->path);
	git__free(entry);
}

static int index_insert(git_index *index, git_index_entry *entry, int replace)
{
	size_t path_length;
	int position;
	git_index_entry **existing = NULL;

	assert(index && entry && entry->path != NULL);

	/* make sure that the path length flag is correct */
	path_length = strlen(entry->path);

	entry->flags &= ~GIT_IDXENTRY_NAMEMASK;

	if (path_length < GIT_IDXENTRY_NAMEMASK)
		entry->flags |= path_length & GIT_IDXENTRY_NAMEMASK;
	else
		entry->flags |= GIT_IDXENTRY_NAMEMASK;;

	/* look if an entry with this path already exists */
	if ((position = git_index_find(index, entry->path)) >= 0) {
		existing = (git_index_entry **)&index->entries.contents[position];

		/* update filemode to existing values if stat is not trusted */
		entry->mode = index_merge_mode(index, *existing, entry->mode);
	}

	/* if replacing is not requested or no existing entry exists, just
	 * insert entry at the end; the index is no longer sorted
	 */
	if (!replace || !existing)
		return git_vector_insert(&index->entries, entry);

	/* exists, replace it */
	git__free((*existing)->path);
	git__free(*existing);
	*existing = entry;

	return 0;
}

static int index_add(git_index *index, const char *path, int stage, int replace)
{
	git_index_entry *entry = NULL;
	int ret;

	if ((ret = index_entry_init(&entry, index, path, stage)) < 0 ||
		(ret = index_insert(index, entry, replace)) < 0)
	{
		index_entry_free(entry);
		return ret;
	}

	git_tree_cache_invalidate_path(index->tree, entry->path);
	return 0;
}

int git_index_add(git_index *index, const char *path, int stage)
{
	return index_add(index, path, stage, 1);
}

int git_index_append(git_index *index, const char *path, int stage)
{
	return index_add(index, path, stage, 0);
}

static int index_add2(
	git_index *index, const git_index_entry *source_entry, int replace)
{
	git_index_entry *entry = NULL;
	int ret;

	entry = index_entry_dup(source_entry);
	if (entry == NULL)
		return -1;

	if ((ret = index_insert(index, entry, replace)) < 0) {
		index_entry_free(entry);
		return ret;
	}

	git_tree_cache_invalidate_path(index->tree, entry->path);
	return 0;
}

int git_index_add2(git_index *index, const git_index_entry *source_entry)
{
	return index_add2(index, source_entry, 1);
}

int git_index_append2(git_index *index, const git_index_entry *source_entry)
{
	return index_add2(index, source_entry, 0);
}

int git_index_remove(git_index *index, int position)
{
	int error;
	git_index_entry *entry;

	git_vector_sort(&index->entries);

	entry = git_vector_get(&index->entries, position);
	if (entry != NULL)
		git_tree_cache_invalidate_path(index->tree, entry->path);

	error = git_vector_remove(&index->entries, (unsigned int)position);

	if (!error)
		index_entry_free(entry);

	return error;
}

int git_index_find(git_index *index, const char *path)
{
	return git_vector_bsearch2(&index->entries, index_srch, path);
}

unsigned int git_index__prefix_position(git_index *index, const char *path)
{
	unsigned int pos;

	git_vector_bsearch3(&pos, &index->entries, index_srch, path);

	return pos;
}

void git_index_uniq(git_index *index)
{
	git_vector_uniq(&index->entries);
}

const git_index_entry_unmerged *git_index_get_unmerged_bypath(
	git_index *index, const char *path)
{
	int pos;
	assert(index && path);

	if (!index->unmerged.length)
		return NULL;

	if ((pos = git_vector_bsearch2(&index->unmerged, unmerged_srch, path)) < 0)
		return NULL;

	return git_vector_get(&index->unmerged, pos);
}

const git_index_entry_unmerged *git_index_get_unmerged_byindex(
	git_index *index, size_t n)
{
	assert(index);
	return git_vector_get(&index->unmerged, n);
}

int index_error_invalid(const char *message)
{
	giterr_set(GITERR_INDEX, "Invalid data in index - %s", message);
	return -1;
}

static void set_index_operations(git_index *index)
{
	if (index->version < INDEX_VERSION_NUMBER ||
		index->version > INDEX_VERSION_NUMBER_EXT) {
		index->version = INDEX_VERSION_DEFAULT;
	}
	switch (index->version) {
		case 2:
		case 3:
			index->operations = &git_index_v2_ops;
			break;
		default:
	        index->operations = NULL;
			break;
	}

}

static int init_by_file_version(git_index *index, const void *buffer, size_t buffer_size)
{
    struct index_header dest;
	const struct index_header *source = buffer;

	if (buffer_size < sizeof (struct index_header))
		return index_error_invalid("insufficient buffer space");

	dest.signature = ntohl(source->signature);
	if (dest.signature != INDEX_HEADER_SIG)
		return index_error_invalid("incorrect header signature");

	dest.version = ntohl(source->version);
	if (dest.version < INDEX_VERSION_NUMBER ||
		dest.version > INDEX_VERSION_NUMBER_EXT)
		return index_error_invalid("incorrect header version");

    index->version = dest.version;

	set_index_operations(index);

	return 0;
}

static int parse_index(git_index *index, const char *buffer, size_t buffer_size)
{
	if (init_by_file_version(index, buffer, buffer_size) < 0) {
		return -1;
	}
	return index->operations->parse(index, buffer, buffer_size);
}

int git_index_entry_stage(const git_index_entry *entry)
{
	return (entry->flags & GIT_IDXENTRY_STAGEMASK) >> GIT_IDXENTRY_STAGESHIFT;
}

typedef struct read_tree_data {
	git_index *index;
	git_indexer_stats *stats;
} read_tree_data;

static int read_tree_cb(const char *root, const git_tree_entry *tentry, void *data)
{
	read_tree_data *rtd = data;
	git_index_entry *entry = NULL;
	git_buf path = GIT_BUF_INIT;

	rtd->stats->total++;

	if (git_tree_entry__is_tree(tentry))
		return 0;

	if (git_buf_joinpath(&path, root, tentry->filename) < 0)
		return -1;

	entry = git__calloc(1, sizeof(git_index_entry));
	GITERR_CHECK_ALLOC(entry);

	entry->mode = tentry->attr;
	entry->oid = tentry->oid;
	entry->path = git_buf_detach(&path);
	git_buf_free(&path);

	if (index_insert(rtd->index, entry, 0) < 0) {
		index_entry_free(entry);
		return -1;
	}

	return 0;
}

int git_index_read_tree(git_index *index, git_tree *tree, git_indexer_stats *stats)
{
	git_indexer_stats dummy_stats;
	read_tree_data rtd = {index, NULL};

	if (!stats) stats = &dummy_stats;
	stats->total = 0;
	rtd.stats = stats;

	git_index_clear(index);

	return git_tree_walk(tree, read_tree_cb, GIT_TREEWALK_POST, &rtd);
}
