/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include <stddef.h>

#include "common.h"
#include "repository.h"
#include "index.h"
#include "tree.h"
#include "tree-cache.h"
#include "hash.h"
#include "git2/odb.h"
#include "git2/blob.h"

#define entry_size(type,len) ((offsetof(type, path) + (len) + 8) & ~7)
#define short_entry_size(len) entry_size(struct entry_short, len)
#define long_entry_size(len) entry_size(struct entry_long, len)

#define minimal_entry_size (offsetof(struct entry_short, path))

static const size_t INDEX_FOOTER_SIZE = GIT_OID_RAWSZ;
static const size_t INDEX_HEADER_SIZE = 12;

static const unsigned int INDEX_VERSION_NUMBER = 2;
static const unsigned int INDEX_VERSION_NUMBER_EXT = 3;

static const unsigned int INDEX_HEADER_SIG = 0x44495243;
static const char INDEX_EXT_TREECACHE_SIG[] = {'T', 'R', 'E', 'E'};
static const char INDEX_EXT_UNMERGED_SIG[] = {'R', 'E', 'U', 'C'};

#define INDEX_OWNER(idx) ((git_repository *)(GIT_REFCOUNT_OWNER(idx)))

struct index_header {
	uint32_t signature;
	uint32_t version;
	uint32_t entry_count;
};

struct index_extension {
	char signature[4];
	uint32_t extension_size;
};

struct entry_time {
	uint32_t seconds;
	uint32_t nanoseconds;
};

struct entry_short {
	struct entry_time ctime;
	struct entry_time mtime;
	uint32_t dev;
	uint32_t ino;
	uint32_t mode;
	uint32_t uid;
	uint32_t gid;
	uint32_t file_size;
	git_oid oid;
	uint16_t flags;
	char path[1]; /* arbitrary length */
};

struct entry_long {
	struct entry_time ctime;
	struct entry_time mtime;
	uint32_t dev;
	uint32_t ino;
	uint32_t mode;
	uint32_t uid;
	uint32_t gid;
	uint32_t file_size;
	git_oid oid;
	uint16_t flags;
	uint16_t flags_extended;
	char path[1]; /* arbitrary length */
};

/* local declarations */
static size_t read_extension(git_index *index, const char *buffer, size_t buffer_size);
static size_t read_entry(git_index_entry *dest, const void *buffer, size_t buffer_size);
static int read_header(struct index_header *dest, const void *buffer);

static int parse_index(git_index *index, const char *buffer, size_t buffer_size);
static int is_index_extended(git_index *index);
static int write_index(git_index *index, git_filebuf *file);

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

static int unmerged_cmp(const void *a, const void *b)
{
	const git_index_entry_unmerged *info_a = a;
	const git_index_entry_unmerged *info_b = b;

	return strcmp(info_a->path, info_b->path);
}

static unsigned int index_create_mode(unsigned int mode)
{
	if (S_ISLNK(mode))
		return S_IFLNK;
	if (S_ISDIR(mode) || (mode & S_IFMT) == (S_IFLNK | S_IFDIR))
		return (S_IFLNK | S_IFDIR);
	return S_IFREG | ((mode & 0100) ? 0755 : 0644);
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

	git_vector_sort(&index->entries);

	if ((error = git_filebuf_open(
			 &file, index->index_file_path, GIT_FILEBUF_HASH_CONTENTS)) < 0)
		return error;

	if ((error = write_index(index, &file)) < 0) {
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
	return index->entries.length;
}

unsigned int git_index_entrycount_unmerged(git_index *index)
{
	assert(index);
	return index->unmerged.length;
}

git_index_entry *git_index_get(git_index *index, unsigned int n)
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
	git_index_entry **entry_array;

	assert(index && entry && entry->path != NULL);

	/* make sure that the path length flag is correct */
	path_length = strlen(entry->path);

	entry->flags &= ~GIT_IDXENTRY_NAMEMASK;

	if (path_length < GIT_IDXENTRY_NAMEMASK)
		entry->flags |= path_length & GIT_IDXENTRY_NAMEMASK;
	else
		entry->flags |= GIT_IDXENTRY_NAMEMASK;;

	/*
	 * replacing is not requested: just insert entry at the end;
	 * the index is no longer sorted
	 */
	if (!replace)
		return git_vector_insert(&index->entries, entry);

	/* look if an entry with this path already exists */
	position = git_index_find(index, entry->path);

	/*
	 * if no entry exists add the entry at the end;
	 * the index is no longer sorted
	 */
	if (position == GIT_ENOTFOUND)
		return git_vector_insert(&index->entries, entry);

	/* exists, replace it */
	entry_array = (git_index_entry **) index->entries.contents;
	git__free(entry_array[position]->path);
	git__free(entry_array[position]);
	entry_array[position] = entry;

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
	return index_add2(index, source_entry, 1);
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
	git_index *index, unsigned int n)
{
	assert(index);
	return git_vector_get(&index->unmerged, n);
}

static int index_error_invalid(const char *message)
{
	giterr_set(GITERR_INDEX, "Invalid data in index - %s", message);
	return -1;
}

static int read_unmerged(git_index *index, const char *buffer, size_t size)
{
	const char *endptr;
	size_t len;
	int i;

	if (git_vector_init(&index->unmerged, 16, unmerged_cmp) < 0)
		return -1;

	while (size) {
		git_index_entry_unmerged *lost;

		len = strlen(buffer) + 1;
		if (size <= len)
			return index_error_invalid("reading unmerged entries");

		lost = git__malloc(sizeof(git_index_entry_unmerged));
		GITERR_CHECK_ALLOC(lost);

		if (git_vector_insert(&index->unmerged, lost) < 0)
			return -1;

		/* read NUL-terminated pathname for entry */
		lost->path = git__strdup(buffer);
		GITERR_CHECK_ALLOC(lost->path);

		size -= len;
		buffer += len;

		/* read 3 ASCII octal numbers for stage entries */
		for (i = 0; i < 3; i++) {
			int tmp;

			if (git__strtol32(&tmp, buffer, &endptr, 8) < 0 ||
				!endptr || endptr == buffer || *endptr ||
				(unsigned)tmp > UINT_MAX)
				return index_error_invalid("reading unmerged entry stage");

			lost->mode[i] = tmp;

			len = (endptr + 1) - buffer;
			if (size <= len)
				return index_error_invalid("reading unmerged entry stage");

			size -= len;
			buffer += len;
		}

		/* read up to 3 OIDs for stage entries */
		for (i = 0; i < 3; i++) {
			if (!lost->mode[i])
				continue;
			if (size < 20)
				return index_error_invalid("reading unmerged entry oid");

			git_oid_fromraw(&lost->oid[i], (const unsigned char *) buffer);
			size -= 20;
			buffer += 20;
		}
	}

	return 0;
}

static size_t read_entry(git_index_entry *dest, const void *buffer, size_t buffer_size)
{
	size_t path_length, entry_size;
	uint16_t flags_raw;
	const char *path_ptr;
	const struct entry_short *source = buffer;

	if (INDEX_FOOTER_SIZE + minimal_entry_size > buffer_size)
		return 0;

	memset(dest, 0x0, sizeof(git_index_entry));

	dest->ctime.seconds = (git_time_t)ntohl(source->ctime.seconds);
	dest->ctime.nanoseconds = ntohl(source->ctime.nanoseconds);
	dest->mtime.seconds = (git_time_t)ntohl(source->mtime.seconds);
	dest->mtime.nanoseconds = ntohl(source->mtime.nanoseconds);
	dest->dev = ntohl(source->dev);
	dest->ino = ntohl(source->ino);
	dest->mode = ntohl(source->mode);
	dest->uid = ntohl(source->uid);
	dest->gid = ntohl(source->gid);
	dest->file_size = ntohl(source->file_size);
	git_oid_cpy(&dest->oid, &source->oid);
	dest->flags = ntohs(source->flags);

	if (dest->flags & GIT_IDXENTRY_EXTENDED) {
		const struct entry_long *source_l = (const struct entry_long *)source;
		path_ptr = source_l->path;

		flags_raw = ntohs(source_l->flags_extended);
		memcpy(&dest->flags_extended, &flags_raw, 2);
	} else
		path_ptr = source->path;

	path_length = dest->flags & GIT_IDXENTRY_NAMEMASK;

	/* if this is a very long string, we must find its
	 * real length without overflowing */
	if (path_length == 0xFFF) {
		const char *path_end;

		path_end = memchr(path_ptr, '\0', buffer_size);
		if (path_end == NULL)
			return 0;

		path_length = path_end - path_ptr;
	}

	if (dest->flags & GIT_IDXENTRY_EXTENDED)
		entry_size = long_entry_size(path_length);
	else
		entry_size = short_entry_size(path_length);

	if (INDEX_FOOTER_SIZE + entry_size > buffer_size)
		return 0;

	dest->path = git__strdup(path_ptr);
	assert(dest->path);

	return entry_size;
}

static int read_header(struct index_header *dest, const void *buffer)
{
	const struct index_header *source = buffer;

	dest->signature = ntohl(source->signature);
	if (dest->signature != INDEX_HEADER_SIG)
		return index_error_invalid("incorrect header signature");

	dest->version = ntohl(source->version);
	if (dest->version != INDEX_VERSION_NUMBER_EXT &&
		dest->version != INDEX_VERSION_NUMBER)
		return index_error_invalid("incorrect header version");

	dest->entry_count = ntohl(source->entry_count);
	return 0;
}

static size_t read_extension(git_index *index, const char *buffer, size_t buffer_size)
{
	const struct index_extension *source;
	struct index_extension dest;
	size_t total_size;

	source = (const struct index_extension *)(buffer);

	memcpy(dest.signature, source->signature, 4);
	dest.extension_size = ntohl(source->extension_size);

	total_size = dest.extension_size + sizeof(struct index_extension);

	if (buffer_size - total_size < INDEX_FOOTER_SIZE)
		return 0;

	/* optional extension */
	if (dest.signature[0] >= 'A' && dest.signature[0] <= 'Z') {
		/* tree cache */
		if (memcmp(dest.signature, INDEX_EXT_TREECACHE_SIG, 4) == 0) {
			if (git_tree_cache_read(&index->tree, buffer + 8, dest.extension_size) < 0)
				return 0;
		} else if (memcmp(dest.signature, INDEX_EXT_UNMERGED_SIG, 4) == 0) {
			if (read_unmerged(index, buffer + 8, dest.extension_size) < 0)
				return 0;
		}
		/* else, unsupported extension. We cannot parse this, but we can skip
		 * it by returning `total_size */
	} else {
		/* we cannot handle non-ignorable extensions;
		 * in fact they aren't even defined in the standard */
		return 0;
	}

	return total_size;
}

static int parse_index(git_index *index, const char *buffer, size_t buffer_size)
{
	unsigned int i;
	struct index_header header;
	git_oid checksum_calculated, checksum_expected;

#define seek_forward(_increase) { \
	if (_increase >= buffer_size) \
		return index_error_invalid("ran out of data while parsing"); \
	buffer += _increase; \
	buffer_size -= _increase;\
}

	if (buffer_size < INDEX_HEADER_SIZE + INDEX_FOOTER_SIZE)
		return index_error_invalid("insufficient buffer space");

	/* Precalculate the SHA1 of the files's contents -- we'll match it to
	 * the provided SHA1 in the footer */
	git_hash_buf(&checksum_calculated, buffer, buffer_size - INDEX_FOOTER_SIZE);

	/* Parse header */
	if (read_header(&header, buffer) < 0)
		return -1;

	seek_forward(INDEX_HEADER_SIZE);

	git_vector_clear(&index->entries);

	/* Parse all the entries */
	for (i = 0; i < header.entry_count && buffer_size > INDEX_FOOTER_SIZE; ++i) {
		size_t entry_size;
		git_index_entry *entry;

		entry = git__malloc(sizeof(git_index_entry));
		GITERR_CHECK_ALLOC(entry);

		entry_size = read_entry(entry, buffer, buffer_size);

		/* 0 bytes read means an object corruption */
		if (entry_size == 0)
			return index_error_invalid("invalid entry");

		if (git_vector_insert(&index->entries, entry) < 0)
			return -1;

		seek_forward(entry_size);
	}

	if (i != header.entry_count)
		return index_error_invalid("header entries changed while parsing");

	/* There's still space for some extensions! */
	while (buffer_size > INDEX_FOOTER_SIZE) {
		size_t extension_size;

		extension_size = read_extension(index, buffer, buffer_size);

		/* see if we have read any bytes from the extension */
		if (extension_size == 0)
			return index_error_invalid("extension size is zero");

		seek_forward(extension_size);
	}

	if (buffer_size != INDEX_FOOTER_SIZE)
		return index_error_invalid("buffer size does not match index footer size");

	/* 160-bit SHA-1 over the content of the index file before this checksum. */
	git_oid_fromraw(&checksum_expected, (const unsigned char *)buffer);

	if (git_oid_cmp(&checksum_calculated, &checksum_expected) != 0)
		return index_error_invalid("calculated checksum does not match expected");

#undef seek_forward

	/* force sorting in the vector: the entries are
	 * assured to be sorted on the index */
	index->entries.sorted = 1;
	return 0;
}

static int is_index_extended(git_index *index)
{
	unsigned int i, extended;
	git_index_entry *entry;

	extended = 0;

	git_vector_foreach(&index->entries, i, entry) {
		entry->flags &= ~GIT_IDXENTRY_EXTENDED;
		if (entry->flags_extended & GIT_IDXENTRY_EXTENDED_FLAGS) {
			extended++;
			entry->flags |= GIT_IDXENTRY_EXTENDED;
		}
	}

	return extended;
}

static int write_disk_entry(git_filebuf *file, git_index_entry *entry)
{
	void *mem = NULL;
	struct entry_short *ondisk;
	size_t path_len, disk_size;
	char *path;

	path_len = strlen(entry->path);

	if (entry->flags & GIT_IDXENTRY_EXTENDED)
		disk_size = long_entry_size(path_len);
	else
		disk_size = short_entry_size(path_len);

	if (git_filebuf_reserve(file, &mem, disk_size) < 0)
		return -1;

	ondisk = (struct entry_short *)mem;

	memset(ondisk, 0x0, disk_size);

	/**
	 * Yes, we have to truncate.
	 *
	 * The on-disk format for Index entries clearly defines
	 * the time and size fields to be 4 bytes each -- so even if
	 * we store these values with 8 bytes on-memory, they must
	 * be truncated to 4 bytes before writing to disk.
	 *
	 * In 2038 I will be either too dead or too rich to care about this
	 */
	ondisk->ctime.seconds = htonl((uint32_t)entry->ctime.seconds);
	ondisk->mtime.seconds = htonl((uint32_t)entry->mtime.seconds);
	ondisk->ctime.nanoseconds = htonl(entry->ctime.nanoseconds);
	ondisk->mtime.nanoseconds = htonl(entry->mtime.nanoseconds);
	ondisk->dev = htonl(entry->dev);
	ondisk->ino = htonl(entry->ino);
	ondisk->mode = htonl(entry->mode);
	ondisk->uid = htonl(entry->uid);
	ondisk->gid = htonl(entry->gid);
	ondisk->file_size = htonl((uint32_t)entry->file_size);

	git_oid_cpy(&ondisk->oid, &entry->oid);

	ondisk->flags = htons(entry->flags);

	if (entry->flags & GIT_IDXENTRY_EXTENDED) {
		struct entry_long *ondisk_ext;
		ondisk_ext = (struct entry_long *)ondisk;
		ondisk_ext->flags_extended = htons(entry->flags_extended);
		path = ondisk_ext->path;
	}
	else
		path = ondisk->path;

	memcpy(path, entry->path, path_len);

	return 0;
}

static int write_entries(git_index *index, git_filebuf *file)
{
	unsigned int i;

	for (i = 0; i < index->entries.length; ++i) {
		git_index_entry *entry;
		entry = git_vector_get(&index->entries, i);
		if (write_disk_entry(file, entry) < 0)
			return -1;
	}

	return 0;
}

static int write_index(git_index *index, git_filebuf *file)
{
	git_oid hash_final;

	struct index_header header;

	int is_extended;

	assert(index && file);

	is_extended = is_index_extended(index);

	header.signature = htonl(INDEX_HEADER_SIG);
	header.version = htonl(is_extended ? INDEX_VERSION_NUMBER_EXT : INDEX_VERSION_NUMBER);
	header.entry_count = htonl(index->entries.length);

	if (git_filebuf_write(file, &header, sizeof(struct index_header)) < 0)
		return -1;

	if (write_entries(index, file) < 0)
		return -1;

	/* TODO: write extensions (tree cache) */

	/* get out the hash for all the contents we've appended to the file */
	git_filebuf_hash(&hash_final, file);

	/* write it at the end of the file */
	return git_filebuf_write(file, hash_final.id, GIT_OID_RAWSZ);
}

int git_index_entry_stage(const git_index_entry *entry)
{
	return (entry->flags & GIT_IDXENTRY_STAGEMASK) >> GIT_IDXENTRY_STAGESHIFT;
}

static int read_tree_cb(const char *root, git_tree_entry *tentry, void *data)
{
	git_index *index = data;
	git_index_entry *entry = NULL;
	git_buf path = GIT_BUF_INIT;

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

	if (index_insert(index, entry, 0) < 0) {
		index_entry_free(entry);
		return -1;
	}

	return 0;
}

int git_index_read_tree(git_index *index, git_tree *tree)
{
	git_index_clear(index);

	return git_tree_walk(tree, read_tree_cb, GIT_TREEWALK_POST, index);
}
