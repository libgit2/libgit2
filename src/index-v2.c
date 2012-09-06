/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "index.h"
#include "tree-cache.h"
#include "hash.h"
#include "git2/oid.h"

#define entry_size(type,len) ((offsetof(type, path) + (len) + 8) & ~7)
#define short_entry_size(len) entry_size(struct entry_short, len)
#define long_entry_size(len) entry_size(struct entry_long, len)

#define minimal_entry_size (offsetof(struct entry_short, path))

static const size_t INDEX_FOOTER_SIZE = GIT_OID_RAWSZ;
static const size_t INDEX_HEADER_SIZE = 12;

static const char INDEX_EXT_TREECACHE_SIG[] = {'T', 'R', 'E', 'E'};
static const char INDEX_EXT_UNMERGED_SIG[] = {'R', 'E', 'U', 'C'};

struct index_v2_header {
	uint32_t signature;
	uint32_t version;
	uint32_t entry_count;
};

struct index_extension {
	char signature[4];
	uint32_t extension_size;
};

struct entry_short {
	struct index_entry_time ctime;
	struct index_entry_time mtime;
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
	struct index_entry_time ctime;
	struct index_entry_time mtime;
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

static int unmerged_cmp(const void *a, const void *b)
{
	const git_index_entry_unmerged *info_a = a;
	const git_index_entry_unmerged *info_b = b;

	return strcmp(info_a->path, info_b->path);
}

static int read_header(struct index_v2_header *dest, const void *buffer)
{
	const struct index_v2_header *source = buffer;

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

static int parse_v2_index(git_index *index, const char *buffer, size_t buffer_size)
{
	unsigned int i;
	struct index_v2_header header;
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

static int write_v2_index(git_index *index, git_filebuf *file) {
	git_oid hash_final;

	struct index_v2_header header;

	int is_extended;

	assert(index && file);

	git_vector_sort(&index->entries);

	is_extended = is_index_extended(index);

	header.signature = htonl(INDEX_HEADER_SIG);
	header.version = htonl(is_extended ? INDEX_VERSION_NUMBER_EXT : INDEX_VERSION_NUMBER);
	header.entry_count = htonl((uint32_t)index->entries.length);

	if (git_filebuf_write(file, &header, sizeof(struct index_v2_header)) < 0)
		return -1;

	if (write_entries(index, file) < 0)
		return -1;

	/* TODO: write extensions (tree cache) */

	/* get out the hash for all the contents we've appended to the file */
	git_filebuf_hash(&hash_final, file);

	/* write it at the end of the file */
	return git_filebuf_write(file, hash_final.id, GIT_OID_RAWSZ);

	return 0;
}

struct git_index_operations git_index_v2_ops = {
	parse_v2_index,
	write_v2_index
};
