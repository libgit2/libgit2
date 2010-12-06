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

#include <stddef.h>

#include "common.h"
#include "repository.h"
#include "index.h"
#include "hash.h"
#include "git2/odb.h"
#include "git2/blob.h"

#define entry_padding(type, len) (8 - ((offsetof(type, path) + (len)) & 0x7))
#define short_entry_padding(len) entry_padding(struct entry_short, len)
#define long_entry_padding(len) entry_padding(struct entry_long, len)

#define entry_size(type,len) ((offsetof(type, path) + (len) + 8) & ~7)
#define short_entry_size(len) entry_size(struct entry_short, len)
#define long_entry_size(len) entry_size(struct entry_long, len)

#define minimal_entry_size (offsetof(struct entry_short, path))

static const char INDEX_HEADER_SIG[] = {'D', 'I', 'R', 'C'};
static const char INDEX_EXT_TREECACHE_SIG[] = {'T', 'R', 'E', 'E'};

static const size_t INDEX_FOOTER_SIZE = GIT_OID_RAWSZ;
static const size_t INDEX_HEADER_SIZE = 12;

static const unsigned int INDEX_VERSION_NUMBER = 2;

struct index_header {
	uint32_t signature;
	uint32_t version;
	uint32_t entry_count;
};

struct index_extension {
	char signature[4];
	uint32_t extension_size;
};

struct entry_short {
	git_index_time ctime;
	git_index_time mtime;
	uint32_t dev;
	uint32_t ino;
	uint32_t mode;
	uint32_t uid;
	uint32_t gid;
	uint32_t file_size;
	git_oid oid;
	uint16_t flags;
	char path[1]; /* arbritrary length */
};

struct entry_long {
	git_index_time ctime;
	git_index_time mtime;
	uint32_t dev;
	uint32_t ino;
	uint32_t mode;
	uint32_t uid;
	uint32_t gid;
	uint32_t file_size;
	git_oid oid;
	uint16_t flags;
	uint16_t flags_extended;
	char path[1]; /* arbritrary length */
};

/* local declarations */
static size_t read_extension(git_index *index, const char *buffer, size_t buffer_size);
static size_t read_entry(git_index_entry *dest, const void *buffer, size_t buffer_size);
static int read_header(struct index_header *dest, const void *buffer);

static int read_tree(git_index *index, const char *buffer, size_t buffer_size);
static git_index_tree *read_tree_internal(const char **, const char *, git_index_tree *);


int index_srch(const void *key, const void *array_member)
{
	const char *filename = (const char *)key;
	const git_index_entry *entry = *(const git_index_entry **)(array_member);

	return strcmp(filename, entry->path);
}

int index_cmp(const void *a, const void *b)
{
	const git_index_entry *entry_a = *(const git_index_entry **)(a);
	const git_index_entry *entry_b = *(const git_index_entry **)(b);

	return strcmp(entry_a->path, entry_b->path);
}


static int index_initialize(git_index **index_out, git_repository *owner, const char *index_path)
{
	git_index *index;

	assert(index_out && index_path);

	index = git__malloc(sizeof(git_index));
	if (index == NULL)
		return GIT_ENOMEM;

	memset(index, 0x0, sizeof(git_index));

	index->index_file_path = git__strdup(index_path);
	if (index->index_file_path == NULL) {
		free(index);
		return GIT_ENOMEM;
	}

	index->repository = owner;

	git_vector_init(&index->entries, 32, index_cmp, index_srch);

	/* Check if index file is stored on disk already */
	if (gitfo_exists(index->index_file_path) == 0)
		index->on_disk = 1;

	*index_out = index;
	return GIT_SUCCESS;
}

int git_index_open_bare(git_index **index_out, const char *index_path)
{
	return index_initialize(index_out, NULL, index_path);
}

int git_index_open_inrepo(git_index **index_out, git_repository *repo)
{
	if (repo->is_bare)
		return GIT_EBAREINDEX;

	return index_initialize(index_out, repo, repo->path_index);
}

void git_index_clear(git_index *index)
{
	unsigned int i;

	assert(index);

	for (i = 0; i < index->entries.length; ++i) {
		git_index_entry *e;
		e = git_vector_get(&index->entries, i);
		free(e->path);
		free(e);
	}

	git_vector_clear(&index->entries);
	index->last_modified = 0;
	index->sorted = 1;

	git_index_tree__free(index->tree);
	index->tree = NULL;
}

void git_index_free(git_index *index)
{
	if (index == NULL)
		return;

	git_index_clear(index);
	git_vector_free(&index->entries);

	free(index->index_file_path);
	free(index);
}


int git_index_read(git_index *index)
{
	struct stat indexst;
	int error = GIT_SUCCESS;

	assert(index->index_file_path);

	if (!index->on_disk || gitfo_exists(index->index_file_path) < 0) {
		git_index_clear(index);
		index->on_disk = 0;
		return GIT_SUCCESS;
	}

	if (gitfo_stat(index->index_file_path, &indexst) < 0)
		return GIT_EOSERR;

	if (!S_ISREG(indexst.st_mode))
		return GIT_ENOTFOUND;

	if (indexst.st_mtime != index->last_modified) {

		gitfo_buf buffer;

		if (gitfo_read_file(&buffer, index->index_file_path) < GIT_SUCCESS)
			return GIT_EOSERR;

		git_index_clear(index);
		error = git_index__parse(index, buffer.data, buffer.len);

		if (error == GIT_SUCCESS)
			index->last_modified = indexst.st_mtime;

		gitfo_free_buf(&buffer);
	}

	return error;
}

int git_index_write(git_index *index)
{
	git_filelock file;
	struct stat indexst;

	if (!index->sorted)
		git_index__sort(index);

	if (git_filelock_init(&file, index->index_file_path) < GIT_SUCCESS)
		return GIT_EFLOCKFAIL;

	if (git_filelock_lock(&file, 0) < GIT_SUCCESS)
		return GIT_EFLOCKFAIL;

	if (git_index__write(index, &file) < GIT_SUCCESS) {
		git_filelock_unlock(&file);
		return GIT_EOSERR;
	}

	if (git_filelock_commit(&file) < GIT_SUCCESS)
		return GIT_EFLOCKFAIL;

	if (gitfo_stat(index->index_file_path, &indexst) == 0) {
		index->last_modified = indexst.st_mtime;
		index->on_disk = 1;
	}

	return GIT_SUCCESS;
}

unsigned int git_index_entrycount(git_index *index)
{
	assert(index);
	return index->entries.length;
}

git_index_entry *git_index_get(git_index *index, int n)
{
	assert(index);
	git_index__sort(index);
	return git_vector_get(&index->entries, (unsigned int)n);
}

int git_index_add(git_index *index, const char *rel_path, int stage)
{
	git_index_entry entry;
	char full_path[GIT_PATH_MAX];
	struct stat st;
	int error;

	if (index->repository == NULL)
		return GIT_EBAREINDEX;

	strcpy(full_path, index->repository->path_workdir);
	strcat(full_path, rel_path);

	if (gitfo_exists(full_path) < 0)
		return GIT_ENOTFOUND;

	if (gitfo_stat(full_path, &st) < 0)
		return GIT_EOSERR;

	if (stage < 0 || stage > 3)
		return GIT_ERROR;

	memset(&entry, 0x0, sizeof(git_index_entry));

	entry.ctime.seconds = st.st_ctime;
	entry.mtime.seconds = st.st_mtime;
	/* entry.mtime.nanoseconds = st.st_mtimensec; */
	/* entry.ctime.nanoseconds = st.st_ctimensec; */
	entry.dev= st.st_rdev;
	entry.ino = st.st_ino;
	entry.mode = st.st_mode;
	entry.uid = st.st_uid;
	entry.gid = st.st_gid;
	entry.file_size = st.st_size;

	/* write the blob to disk and get the oid */
	if ((error = git_blob_writefile(&entry.oid, index->repository, full_path)) < GIT_SUCCESS)
		return error;

	entry.flags |= (stage << GIT_IDXENTRY_STAGESHIFT);
	entry.path = (char *)rel_path; /* do not duplicate; index_insert already does this */

	return git_index_insert(index, &entry);
}

void git_index__sort(git_index *index)
{
	if (index->sorted == 0) {
		git_vector_sort(&index->entries);
		index->sorted = 1;
	}
}

int git_index_insert(git_index *index, const git_index_entry *source_entry)
{
	git_index_entry *entry;
	size_t path_length;
	int position;

	assert(index && source_entry);

	if (source_entry->path == NULL)
		return GIT_EMISSINGOBJDATA;

	entry = git__malloc(sizeof(git_index_entry));
	if (entry == NULL)
		return GIT_ENOMEM;

	memcpy(entry, source_entry, sizeof(git_index_entry));

	/* duplicate the path string so we own it */
	entry->path = git__strdup(entry->path);
	if (entry->path == NULL)
		return GIT_ENOMEM;

	/* make sure that the path length flag is correct */
	path_length = strlen(entry->path);

	entry->flags &= ~GIT_IDXENTRY_NAMEMASK;

	if (path_length < GIT_IDXENTRY_NAMEMASK)
		entry->flags |= path_length & GIT_IDXENTRY_NAMEMASK;
	else
		entry->flags |= GIT_IDXENTRY_NAMEMASK;;


	/* look if an entry with this path already exists */
	position = git_index_find(index, source_entry->path);

	/* if no entry exists, add the entry at the end;
	 * the index is no longer sorted */
	if (position == GIT_ENOTFOUND) {
		if (git_vector_insert(&index->entries, entry) < GIT_SUCCESS)
			return GIT_ENOMEM;

		index->sorted = 0;

	/* if a previous entry exists, replace it */
	} else {
		git_index_entry **entry_array = (git_index_entry **)index->entries.contents;

		free(entry_array[position]->path);
		free(entry_array[position]);

		entry_array[position] = entry;
	}

	return GIT_SUCCESS;
}

int git_index_remove(git_index *index, int position)
{
	assert(index);
	git_index__sort(index);
	return git_vector_remove(&index->entries, (unsigned int)position);
}

int git_index_find(git_index *index, const char *path)
{
	git_index__sort(index);
	return git_vector_search(&index->entries, path);
}

void git_index_tree__free(git_index_tree *tree)
{
	unsigned int i;

	if (tree == NULL)
		return;

	for (i = 0; i < tree->children_count; ++i)
		git_index_tree__free(tree->children[i]);

	free(tree->name);
	free(tree->children);
	free(tree);
}

static git_index_tree *read_tree_internal(
		const char **buffer_in, const char *buffer_end, git_index_tree *parent)
{
	git_index_tree *tree;
	const char *name_start, *buffer;

	if ((tree = git__malloc(sizeof(git_index_tree))) == NULL)
		return NULL;

	memset(tree, 0x0, sizeof(git_index_tree));
	tree->parent = parent;

	buffer = name_start = *buffer_in;

	if ((buffer = memchr(buffer, '\0', buffer_end - buffer)) == NULL)
		goto error_cleanup;

	/* NUL-terminated tree name */
	tree->name = git__strdup(name_start);
	if (++buffer >= buffer_end)
		goto error_cleanup;

	/* Blank-terminated ASCII decimal number of entries in this tree */
	tree->entries = strtol(buffer, (char **)&buffer, 10);
	if (*buffer != ' ' || ++buffer >= buffer_end)
		goto error_cleanup;

	 /* Number of children of the tree, newline-terminated */
	tree->children_count = strtol(buffer, (char **)&buffer, 10);
	if (*buffer != '\n' || ++buffer >= buffer_end)
		goto error_cleanup;

	/* 160-bit SHA-1 for this tree and it's children */
	if (buffer + GIT_OID_RAWSZ > buffer_end)
		goto error_cleanup;

	git_oid_mkraw(&tree->oid, (const unsigned char *)buffer);
	buffer += GIT_OID_RAWSZ;

	/* Parse children: */
	if (tree->children_count > 0) {
		unsigned int i;

		tree->children = git__malloc(tree->children_count * sizeof(git_index_tree *));
		if (tree->children == NULL)
			goto error_cleanup;

		for (i = 0; i < tree->children_count; ++i) {
			tree->children[i] = read_tree_internal(&buffer, buffer_end, tree);

			if (tree->children[i] == NULL)
				goto error_cleanup;
		}
	}

	*buffer_in = buffer;
	return tree;

error_cleanup:
	git_index_tree__free(tree);
	return NULL;
}

static int read_tree(git_index *index, const char *buffer, size_t buffer_size)
{
	const char *buffer_end = buffer + buffer_size;

	index->tree = read_tree_internal(&buffer, buffer_end, NULL);
	return (index->tree != NULL && buffer == buffer_end) ? GIT_SUCCESS : GIT_EOBJCORRUPTED;
}

static size_t read_entry(git_index_entry *dest, const void *buffer, size_t buffer_size)
{
	size_t path_length, entry_size;
	uint16_t flags_raw;
	const char *path_ptr;
	const struct entry_short *source;

	if (INDEX_FOOTER_SIZE + minimal_entry_size > buffer_size)
		return 0;

	memset(dest, 0x0, sizeof(git_index_entry));

	source = (const struct entry_short *)(buffer);

	dest->ctime.seconds = ntohl(source->ctime.seconds);
	dest->ctime.nanoseconds = ntohl(source->ctime.nanoseconds);
	dest->mtime.seconds = ntohl(source->mtime.seconds);
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
		struct entry_long *source_l = (struct entry_long *)source;
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
	const struct index_header *source;
	source = (const struct index_header *)(buffer);

	dest->signature = source->signature;
	if (memcmp(&dest->signature, INDEX_HEADER_SIG, 4) != 0)
		return GIT_EOBJCORRUPTED;

	dest->version = ntohl(source->version);
	if (dest->version != INDEX_VERSION_NUMBER)
		return GIT_EOBJCORRUPTED;

	dest->entry_count = ntohl(source->entry_count);
	return GIT_SUCCESS;
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

			if (read_tree(index, buffer + 8, dest.extension_size) < GIT_SUCCESS)
				return 0;
		}
	} else {
		/* we cannot handle non-ignorable extensions;
		 * in fact they aren't even defined in the standard */
		return 0;
	}

	return total_size;
}

int git_index__parse(git_index *index, const char *buffer, size_t buffer_size)
{
	unsigned int i;
	struct index_header header;
	git_oid checksum_calculated, checksum_expected;

#define seek_forward(_increase) { \
	if (_increase >= buffer_size) \
		return GIT_EOBJCORRUPTED; \
	buffer += _increase; \
	buffer_size -= _increase;\
}

	if (buffer_size < INDEX_HEADER_SIZE + INDEX_FOOTER_SIZE)
		return GIT_EOBJCORRUPTED;

	/* Precalculate the SHA1 of the files's contents -- we'll match it to
	 * the provided SHA1 in the footer */
	git_hash_buf(&checksum_calculated, (const void *)buffer, buffer_size - INDEX_FOOTER_SIZE);

	/* Parse header */
	if (read_header(&header, buffer) < GIT_SUCCESS)
		return GIT_EOBJCORRUPTED;

	seek_forward(INDEX_HEADER_SIZE);

	git_vector_clear(&index->entries);

	/* Parse all the entries */
	for (i = 0; i < header.entry_count && buffer_size > INDEX_FOOTER_SIZE; ++i) {
		size_t entry_size;
		git_index_entry *entry;

		entry = git__malloc(sizeof(git_index_entry));
		if (entry == NULL)
			return GIT_ENOMEM;

		entry_size = read_entry(entry, buffer, buffer_size);

		/* 0 bytes read means an object corruption */
		if (entry_size == 0)
			return GIT_EOBJCORRUPTED;

		if (git_vector_insert(&index->entries, entry) < GIT_SUCCESS)
			return GIT_ENOMEM;

		seek_forward(entry_size);
	}

	if (i != header.entry_count)
		return GIT_EOBJCORRUPTED;

	/* There's still space for some extensions! */
	while (buffer_size > INDEX_FOOTER_SIZE) {
		size_t extension_size;

		extension_size = read_extension(index, buffer, buffer_size);

		/* see if we have read any bytes from the extension */
		if (extension_size == 0)
			return GIT_EOBJCORRUPTED;

		seek_forward(extension_size);
	}

	if (buffer_size != INDEX_FOOTER_SIZE)
		return GIT_EOBJCORRUPTED;

	/* 160-bit SHA-1 over the content of the index file before this checksum. */
	git_oid_mkraw(&checksum_expected, (const unsigned char *)buffer);

	if (git_oid_cmp(&checksum_calculated, &checksum_expected) != 0)
		return GIT_EOBJCORRUPTED;

#undef seek_forward

	return GIT_SUCCESS;
}

int git_index__write(git_index *index, git_filelock *file)
{
	static const char NULL_BYTES[] = {0, 0, 0, 0, 0, 0, 0, 0};

	int error = GIT_SUCCESS;
	unsigned int i;

	git_hash_ctx *digest;
	git_oid hash_final;

	assert(index && file && file->is_locked);

	if ((digest = git_hash_new_ctx()) == NULL)
		return GIT_ENOMEM;

#define WRITE_WORD(_word) {\
	uint32_t network_word = htonl((_word));\
	git_filelock_write(file, &network_word, 4);\
	git_hash_update(digest, &network_word, 4);\
}

#define WRITE_SHORT(_shrt) {\
	uint16_t network_shrt = htons((_shrt));\
	git_filelock_write(file, &network_shrt, 2);\
	git_hash_update(digest, &network_shrt, 2);\
}

#define WRITE_BYTES(_bytes, _n) {\
	git_filelock_write(file, _bytes, _n);\
	git_hash_update(digest, _bytes, _n);\
}

	WRITE_BYTES(INDEX_HEADER_SIG, 4);

	WRITE_WORD(INDEX_VERSION_NUMBER);
	WRITE_WORD(index->entries.length);

	for (i = 0; i < index->entries.length; ++i) {
		git_index_entry *entry;
		size_t path_length, padding;

		entry = git_vector_get(&index->entries, i);
		path_length = strlen(entry->path);

		WRITE_WORD(entry->ctime.seconds);
		WRITE_WORD(entry->ctime.nanoseconds);
		WRITE_WORD(entry->mtime.seconds);
		WRITE_WORD(entry->mtime.nanoseconds);
		WRITE_WORD(entry->dev);
		WRITE_WORD(entry->ino);
		WRITE_WORD(entry->mode);
		WRITE_WORD(entry->uid);
		WRITE_WORD(entry->gid);
		WRITE_WORD(entry->file_size);
		WRITE_BYTES(entry->oid.id, GIT_OID_RAWSZ);

		if (entry->flags_extended != 0)
			entry->flags |= GIT_IDXENTRY_EXTENDED;

		WRITE_SHORT(entry->flags);

		if (entry->flags & GIT_IDXENTRY_EXTENDED) {
			WRITE_SHORT(entry->flags_extended);
			padding = long_entry_padding(path_length);
		} else
			padding = short_entry_padding(path_length);

		WRITE_BYTES(entry->path, path_length);
		WRITE_BYTES(NULL_BYTES, padding);
	}

#undef WRITE_WORD
#undef WRITE_BYTES
#undef WRITE_SHORT
#undef WRITE_FLAGS

	/* TODO: write extensions (tree cache) */

	git_hash_final(&hash_final, digest);
	git_hash_free_ctx(digest);
	git_filelock_write(file, hash_final.id, GIT_OID_RAWSZ);

	return error;
}
