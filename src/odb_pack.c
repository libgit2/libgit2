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
#include "git2/zlib.h"
#include "git2/repository.h"
#include "git2/oid.h"
#include "fileops.h"
#include "hash.h"
#include "odb.h"
#include "delta-apply.h"
#include "sha1_lookup.h"

#include "git2/odb_backend.h"

#define DEFAULT_WINDOW_SIZE \
	(sizeof(void*) >= 8 \
		?  1 * 1024 * 1024 * 1024 \
		: 32 * 1024 * 1024)

#define DEFAULT_MAPPED_LIMIT \
	((1024L * 1024L) * (sizeof(void*) >= 8 ? 8192 : 256))

#define PACK_SIGNATURE 0x5041434b	/* "PACK" */
#define PACK_VERSION 2
#define pack_version_ok(v) ((v) == htonl(2) || (v) == htonl(3))
struct pack_header {
	uint32_t hdr_signature;
	uint32_t hdr_version;
	uint32_t hdr_entries;
};

/*
 * The first four bytes of index formats later than version 1 should
 * start with this signature, as all older git binaries would find this
 * value illegal and abort reading the file.
 *
 * This is the case because the number of objects in a packfile
 * cannot exceed 1,431,660,000 as every object would need at least
 * 3 bytes of data and the overall packfile cannot exceed 4 GiB with
 * version 1 of the index file due to the offsets limited to 32 bits.
 * Clearly the signature exceeds this maximum.
 *
 * Very old git binaries will also compare the first 4 bytes to the
 * next 4 bytes in the index and abort with a "non-monotonic index"
 * error if the second 4 byte word is smaller than the first 4
 * byte word.  This would be true in the proposed future index
 * format as idx_signature would be greater than idx_version.
 */
#define PACK_IDX_SIGNATURE 0xff744f63	/* "\377tOc" */

struct pack_idx_header {
	uint32_t idx_signature;
	uint32_t idx_version;
};

struct pack_window {
	struct pack_window *next;
	git_map window_map;
	off_t offset;
	unsigned int last_used;
	unsigned int inuse_cnt;
};

struct pack_file {
	struct pack_window *windows;
	off_t pack_size;

	git_map index_map;

	uint32_t num_objects;
	uint32_t num_bad_objects;
	git_oid *bad_object_sha1; /* array of git_oid */

	int index_version;
	git_time_t mtime;
	int pack_fd;
	unsigned pack_local:1, pack_keep:1;
	git_oid sha1;

	/* something like ".git/objects/pack/xxxxx.pack" */
	char pack_name[GIT_FLEX_ARRAY]; /* more */
};

struct pack_entry {
	off_t offset;
	git_oid sha1;
	struct pack_file *p;
};

struct pack_backend {
	git_odb_backend parent;
	git_vector packs;
	struct pack_file *last_found;
	char *pack_folder;
	time_t pack_folder_mtime;

	size_t window_size; /* needs default value */

	size_t mapped_limit; /* needs default value */
	size_t peak_mapped;
	size_t mapped;

	size_t used_ctr;

	unsigned int peak_open_windows;
	unsigned int open_windows;

	unsigned int mmap_calls;
};

/**
 * The wonderful tale of a Packed Object lookup query
 * ===================================================
 *   A riveting and epic story of epicness and ASCII
 *          art, presented by yours truly,
 *               Sir Vicent of Marti
 *
 *
 *	Chapter 1: Once upon a time...
 *	Initialization of the Pack Backend
 *	--------------------------------------------------
 *
 *	# git_odb_backend_pack
 *	| Creates the pack backend structure, initializes the
 *	| callback pointers to our default read() and exist() methods,
 *	| and tries to preload all the known packfiles in the ODB.
 *  |
 *	|-# packfile_load_all
 *	  | Tries to find the `pack` folder, if it exists. ODBs without
 *	  | a pack folder are ignored altogether. If there's a `pack` folder
 *	  | we run a `dirent` callback through every file in the pack folder
 *	  | to find our packfiles. The packfiles are then sorted according
 *	  | to a sorting callback.
 * 	  |
 *	  |-# packfile_load__cb
 *	  | | This callback is called from `dirent` with every single file
 *	  | | inside the pack folder. We find the packs by actually locating
 *	  | | their index (ends in ".idx"). From that index, we verify that
 *	  | | the corresponding packfile exists and is valid, and if so, we
 *    | | add it to the pack list.
 *	  | |
 *	  | |-# packfile_check
 *	  |     Make sure that there's a packfile to back this index, and store
 *	  |     some very basic information regarding the packfile itself,
 *	  |     such as the full path, the size, and the modification time.
 *	  |     We don't actually open the packfile to check for internal consistency.
 *    |
 *    |-# packfile_sort__cb
 *        Sort all the preloaded packs according to some specific criteria:
 *        we prioritize the "newer" packs because it's more likely they
 *        contain the objects we are looking for, and we prioritize local
 *        packs over remote ones.
 *
 *
 *
 *	Chapter 2: To be, or not to be...
 *	A standard packed `exist` query for an OID
 *	--------------------------------------------------
 *
 *  # pack_backend__exists
 *  | Check if the given SHA1 oid exists in any of the packs
 *  | that have been loaded for our ODB.
 *  |
 *  |-# pack_entry_find
 *    | Iterate through all the packs that have been preloaded
 *    | (starting by the pack where the latest object was found)
 *    | to try to find the OID in one of them.
 *    |
 *    |-# pack_entry_find1
 *      | Check the index of an individual pack to see if the SHA1
 *      | OID can be found. If we can find the offset to that SHA1
 *      | inside of the index, that means the object is contained
 *      | inside of the packfile and we can stop searching.
 *      | Before returning, we verify that the packfile behing the
 *      | index we are searching still exists on disk.
 *      |
 *      |-# pack_entry_find_offset
 *      | | Mmap the actual index file to disk if it hasn't been opened
 *      | | yet, and run a binary search through it to find the OID.
 *      | | See <http://book.git-scm.com/7_the_packfile.html> for specifics
 *      | | on the Packfile Index format and how do we find entries in it.
 *      | |
 *      | |-# pack_index_open
 *      |   | Guess the name of the index based on the full path to the
 *      |   | packfile, open it and verify its contents. Only if the index
 *      |   | has not been opened already.
 *      |   |
 *      |   |-# pack_index_check
 *      |       Mmap the index file and do a quick run through the header
 *      |       to guess the index version (right now we support v1 and v2),
 *      |       and to verify that the size of the index makes sense.
 *      |
 *      |-# packfile_open
 *          See `packfile_open` in Chapter 3
 *
 *
 *
 *	Chapter 3: The neverending story...
 *	A standard packed `lookup` query for an OID
 *	--------------------------------------------------
 *	TODO
 *
 */




/***********************************************************
 *
 * FORWARD DECLARATIONS
 *
 ***********************************************************/

static void pack_window_free_all(struct pack_backend *backend, struct pack_file *p);
static int pack_window_contains(struct pack_window *win, off_t offset);

static void pack_window_scan_lru(struct pack_file *p, struct pack_file **lru_p,
		struct pack_window **lru_w, struct pack_window **lru_l);

static int pack_window_close_lru( struct pack_backend *backend,
		struct pack_file *current, git_file keep_fd);

static void pack_window_close(struct pack_window **w_cursor);

static unsigned char *pack_window_open( struct pack_backend *backend,
		struct pack_file *p, struct pack_window **w_cursor, off_t offset,
		unsigned int *left);

static int packfile_sort__cb(const void *a_, const void *b_);

static void pack_index_free(struct pack_file *p);

static int pack_index_check(const char *path,  struct pack_file *p);
static int pack_index_open(struct pack_file *p);

static struct pack_file *packfile_alloc(int extra);
static int packfile_open(struct pack_file *p);
static int packfile_check(struct pack_file **pack_out, const char *path);
static int packfile_load__cb(void *_data, char *path);
static int packfile_refresh_all(struct pack_backend *backend);

static off_t nth_packed_object_offset(const struct pack_file *p, uint32_t n);

/* Can find the offset of an object given
 * a prefix of an identifier.
 * Throws GIT_EAMBIGUOUSOIDPREFIX if short oid
 * is ambiguous within the pack.
 * This method assumes that len is between
 * GIT_OID_MINPREFIXLEN and GIT_OID_HEXSZ.
 */
static int pack_entry_find_offset(
		off_t *offset_out,
		git_oid *found_oid,
		struct pack_file *p,
		const git_oid *short_oid,
		unsigned int len);

static int pack_entry_find1(
		struct pack_entry *e,
		struct pack_file *p,
		const git_oid *short_oid,
		unsigned int len);

static int pack_entry_find(struct pack_entry *e,
		struct pack_backend *backend, const git_oid *oid);

/* Can find the offset of an object given
 * a prefix of an identifier.
 * Throws GIT_EAMBIGUOUSOIDPREFIX if short oid
 * is ambiguous.
 * This method assumes that len is between
 * GIT_OID_MINPREFIXLEN and GIT_OID_HEXSZ.
 */
static int pack_entry_find_prefix(struct pack_entry *e,
					struct pack_backend *backend,
					const git_oid *short_oid,
					unsigned int len);

static off_t get_delta_base(struct pack_backend *backend,
		struct pack_file *p, struct pack_window **w_curs,
		off_t *curpos, git_otype type,
		off_t delta_obj_offset);

static unsigned long packfile_unpack_header1(
		size_t *sizep,
		git_otype *type,
		const unsigned char *buf,
		unsigned long len);

static int packfile_unpack_header(
		size_t *size_p,
		git_otype *type_p,
		struct pack_backend *backend,
		struct pack_file *p,
		struct pack_window **w_curs,
		off_t *curpos);

static int packfile_unpack_compressed(
		git_rawobj *obj,
		struct pack_backend *backend,
		struct pack_file *p,
		struct pack_window **w_curs,
		off_t curpos,
		size_t size,
		git_otype type);

static int packfile_unpack_delta(
		git_rawobj *obj,
		struct pack_backend *backend,
		struct pack_file *p,
		struct pack_window **w_curs,
		off_t curpos,
		size_t delta_size,
		git_otype delta_type,
		off_t obj_offset);

static int packfile_unpack(git_rawobj *obj, struct pack_backend *backend,
		struct pack_file *p, off_t obj_offset);





/***********************************************************
 *
 * PACK WINDOW MANAGEMENT
 *
 ***********************************************************/

void pack_window_free_all(struct pack_backend *backend, struct pack_file *p)
{
	while (p->windows) {
		struct pack_window *w = p->windows;
		assert(w->inuse_cnt == 0);

		backend->mapped -= w->window_map.len;
		backend->open_windows--;

		git_futils_mmap_free(&w->window_map);

		p->windows = w->next;
		free(w);
	}
}

GIT_INLINE(int) pack_window_contains(struct pack_window *win, off_t offset)
{
	/* We must promise at least 20 bytes (one hash) after the
	 * offset is available from this window, otherwise the offset
	 * is not actually in this window and a different window (which
	 * has that one hash excess) must be used.  This is to support
	 * the object header and delta base parsing routines below.
	 */
	off_t win_off = win->offset;
	return win_off <= offset
		&& (offset + 20) <= (off_t)(win_off + win->window_map.len);
}

static void pack_window_scan_lru(
	struct pack_file *p,
	struct pack_file **lru_p,
	struct pack_window **lru_w,
	struct pack_window **lru_l)
{
	struct pack_window *w, *w_l;

	for (w_l = NULL, w = p->windows; w; w = w->next) {
		if (!w->inuse_cnt) {
			if (!*lru_w || w->last_used < (*lru_w)->last_used) {
				*lru_p = p;
				*lru_w = w;
				*lru_l = w_l;
			}
		}
		w_l = w;
	}
}

static int pack_window_close_lru(
		struct pack_backend *backend,
		struct pack_file *current,
		git_file keep_fd)
{
	struct pack_file *lru_p = NULL;
	struct pack_window *lru_w = NULL, *lru_l = NULL;
	size_t i;

	if (current)
		pack_window_scan_lru(current, &lru_p, &lru_w, &lru_l);

	for (i = 0; i < backend->packs.length; ++i)
		pack_window_scan_lru(git_vector_get(&backend->packs, i), &lru_p, &lru_w, &lru_l);

	if (lru_p) {
		backend->mapped -= lru_w->window_map.len;
		git_futils_mmap_free(&lru_w->window_map);

		if (lru_l)
			lru_l->next = lru_w->next;
		else {
			lru_p->windows = lru_w->next;
			if (!lru_p->windows && lru_p->pack_fd != keep_fd) {
				p_close(lru_p->pack_fd);
				lru_p->pack_fd = -1;
			}
		}

		free(lru_w);
		backend->open_windows--;
		return GIT_SUCCESS;
	}

	return git__throw(GIT_ERROR, "Failed to close pack window");
}

static void pack_window_close(struct pack_window **w_cursor)
{
	struct pack_window *w = *w_cursor;
	if (w) {
		w->inuse_cnt--;
		*w_cursor = NULL;
	}
}

static unsigned char *pack_window_open(
		struct pack_backend *backend,
		struct pack_file *p,
		struct pack_window **w_cursor,
		off_t offset,
		unsigned int *left)
{
	struct pack_window *win = *w_cursor;

	if (p->pack_fd == -1 && packfile_open(p) < GIT_SUCCESS)
		return NULL;

	/* Since packfiles end in a hash of their content and it's
	 * pointless to ask for an offset into the middle of that
	 * hash, and the pack_window_contains function above wouldn't match
	 * don't allow an offset too close to the end of the file.
	 */
	if (offset > (p->pack_size - 20))
		return NULL;

	if (!win || !pack_window_contains(win, offset)) {

		if (win)
			win->inuse_cnt--;

		for (win = p->windows; win; win = win->next) {
			if (pack_window_contains(win, offset))
				break;
		}

		if (!win) {
			size_t window_align = backend->window_size / 2;
			size_t len;

			win = git__calloc(1, sizeof(*win));
			win->offset = (offset / window_align) * window_align;

			len = (size_t)(p->pack_size - win->offset);
			if (len > backend->window_size)
				len = backend->window_size;

			backend->mapped += len;

			while (backend->mapped_limit < backend->mapped &&
				pack_window_close_lru(backend, p, p->pack_fd) == GIT_SUCCESS) {}

			if (git_futils_mmap_ro(&win->window_map, p->pack_fd,
					win->offset, len) < GIT_SUCCESS)
				return NULL;

			backend->mmap_calls++;
			backend->open_windows++;

			if (backend->mapped > backend->peak_mapped)
				backend->peak_mapped = backend->mapped;

			if (backend->open_windows > backend->peak_open_windows)
				backend->peak_open_windows = backend->open_windows;

			win->next = p->windows;
			p->windows = win;
		}
	}

	if (win != *w_cursor) {
		win->last_used = backend->used_ctr++;
		win->inuse_cnt++;
		*w_cursor = win;
	}

	offset -= win->offset;
	assert(git__is_sizet(offset));

	if (left)
		*left = win->window_map.len - (size_t)offset;

	return (unsigned char *)win->window_map.data + offset;
}







/***********************************************************
 *
 * PACK INDEX METHODS
 *
 ***********************************************************/

static void pack_index_free(struct pack_file *p)
{
	if (p->index_map.data) {
		git_futils_mmap_free(&p->index_map);
		p->index_map.data = NULL;
	}
}

static int pack_index_check(const char *path,  struct pack_file *p)
{
	struct pack_idx_header *hdr;
	uint32_t version, nr, i, *index;

	void *idx_map;
	size_t idx_size;

	struct stat st;

	/* TODO: properly open the file without access time */
	git_file fd = p_open(path, O_RDONLY /*| O_NOATIME */);

	int error;

	if (fd < 0)
		return git__throw(GIT_EOSERR, "Failed to check index. File missing or corrupted");

	if (p_fstat(fd, &st) < GIT_SUCCESS) {
		p_close(fd);
		return git__throw(GIT_EOSERR, "Failed to check index. File appears to be corrupted");
	}

	if (!git__is_sizet(st.st_size))
		return GIT_ENOMEM;

	idx_size = (size_t)st.st_size;

	if (idx_size < 4 * 256 + 20 + 20) {
		p_close(fd);
		return git__throw(GIT_EOBJCORRUPTED, "Failed to check index. Object is corrupted");
	}

	error = git_futils_mmap_ro(&p->index_map, fd, 0, idx_size);
	p_close(fd);

	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to check index");

	hdr = idx_map = p->index_map.data;

	if (hdr->idx_signature == htonl(PACK_IDX_SIGNATURE)) {
		version = ntohl(hdr->idx_version);

		if (version < 2 || version > 2) {
			git_futils_mmap_free(&p->index_map);
			return git__throw(GIT_EOBJCORRUPTED, "Failed to check index. Unsupported index version");
		}

	} else
		version = 1;

	nr = 0;
	index = idx_map;

	if (version > 1)
		index += 2;  /* skip index header */

	for (i = 0; i < 256; i++) {
		uint32_t n = ntohl(index[i]);
		if (n < nr) {
			git_futils_mmap_free(&p->index_map);
			return git__throw(GIT_EOBJCORRUPTED, "Failed to check index. Index is non-monotonic");
		}
		nr = n;
	}

	if (version == 1) {
		/*
		 * Total size:
		 *  - 256 index entries 4 bytes each
		 *  - 24-byte entries * nr (20-byte sha1 + 4-byte offset)
		 *  - 20-byte SHA1 of the packfile
		 *  - 20-byte SHA1 file checksum
		 */
		if (idx_size != 4*256 + nr * 24 + 20 + 20) {
			git_futils_mmap_free(&p->index_map);
			return git__throw(GIT_EOBJCORRUPTED, "Failed to check index. Object is corrupted");
		}
	} else if (version == 2) {
		/*
		 * Minimum size:
		 *  - 8 bytes of header
		 *  - 256 index entries 4 bytes each
		 *  - 20-byte sha1 entry * nr
		 *  - 4-byte crc entry * nr
		 *  - 4-byte offset entry * nr
		 *  - 20-byte SHA1 of the packfile
		 *  - 20-byte SHA1 file checksum
		 * And after the 4-byte offset table might be a
		 * variable sized table containing 8-byte entries
		 * for offsets larger than 2^31.
		 */
		unsigned long min_size = 8 + 4*256 + nr*(20 + 4 + 4) + 20 + 20;
		unsigned long max_size = min_size;

		if (nr)
			max_size += (nr - 1)*8;

		if (idx_size < min_size || idx_size > max_size) {
			git_futils_mmap_free(&p->index_map);
			return git__throw(GIT_EOBJCORRUPTED, "Failed to check index. Wrong index size");
		}

		/* Make sure that off_t is big enough to access the whole pack...
		 * Is this an issue in libgit2? It shouldn't. */
		if (idx_size != min_size && (sizeof(off_t) <= 4)) {
			git_futils_mmap_free(&p->index_map);
			return git__throw(GIT_EOSERR, "Failed to check index. off_t not big enough to access the whole pack");
		}
	}

	p->index_version = version;
	p->num_objects = nr;
	return GIT_SUCCESS;
}

static int pack_index_open(struct pack_file *p)
{
	char *idx_name;
	int error;

	if (p->index_map.data)
		return GIT_SUCCESS;

	idx_name = git__strdup(p->pack_name);
	strcpy(idx_name + strlen(idx_name) - STRLEN(".pack"), ".idx");

	error = pack_index_check(idx_name, p);
	free(idx_name);

	return error == GIT_SUCCESS ? GIT_SUCCESS : git__rethrow(error, "Failed to open index");
}









/***********************************************************
 *
 * PACKFILE METHODS
 *
 ***********************************************************/

static int packfile_sort__cb(const void *a_, const void *b_)
{
	struct pack_file *a = *((struct pack_file **)a_);
	struct pack_file *b = *((struct pack_file **)b_);
	int st;

	/*
	 * Local packs tend to contain objects specific to our
	 * variant of the project than remote ones.  In addition,
	 * remote ones could be on a network mounted filesystem.
	 * Favor local ones for these reasons.
	 */
	st = a->pack_local - b->pack_local;
	if (st)
		return -st;

	/*
	 * Younger packs tend to contain more recent objects,
	 * and more recent objects tend to get accessed more
	 * often.
	 */
	if (a->mtime < b->mtime)
		return 1;
	else if (a->mtime == b->mtime)
		return 0;

	return -1;
}

static struct pack_file *packfile_alloc(int extra)
{
	struct pack_file *p = git__malloc(sizeof(*p) + extra);
	memset(p, 0, sizeof(*p));
	p->pack_fd = -1;
	return p;
}


static void packfile_free(struct pack_backend *backend, struct pack_file *p)
{
	assert(p);

	/* clear_delta_base_cache(); */
	pack_window_free_all(backend, p);

	if (p->pack_fd != -1)
		p_close(p->pack_fd);

	pack_index_free(p);

	free(p->bad_object_sha1);
	free(p);
}

static int packfile_open(struct pack_file *p)
{
	struct stat st;
	struct pack_header hdr;
	git_oid sha1;
	unsigned char *idx_sha1;

	if (!p->index_map.data && pack_index_open(p) < GIT_SUCCESS)
		return git__throw(GIT_ENOTFOUND, "Failed to open packfile. File not found");

	/* TODO: open with noatime */
	p->pack_fd = p_open(p->pack_name, O_RDONLY);
	if (p->pack_fd < 0 || p_fstat(p->pack_fd, &st) < GIT_SUCCESS)
		return git__throw(GIT_EOSERR, "Failed to open packfile. File appears to be corrupted");

	/* If we created the struct before we had the pack we lack size. */
	if (!p->pack_size) {
		if (!S_ISREG(st.st_mode))
			goto cleanup;
		p->pack_size = (off_t)st.st_size;
	} else if (p->pack_size != st.st_size)
		goto cleanup;

#if 0
	/* We leave these file descriptors open with sliding mmap;
	 * there is no point keeping them open across exec(), though.
	 */
	fd_flag = fcntl(p->pack_fd, F_GETFD, 0);
	if (fd_flag < 0)
		return error("cannot determine file descriptor flags");

	fd_flag |= FD_CLOEXEC;
	if (fcntl(p->pack_fd, F_SETFD, fd_flag) == -1)
		return GIT_EOSERR;
#endif

	/* Verify we recognize this pack file format. */
	if (p_read(p->pack_fd, &hdr, sizeof(hdr)) < GIT_SUCCESS)
		goto cleanup;

	if (hdr.hdr_signature != htonl(PACK_SIGNATURE))
		goto cleanup;

	if (!pack_version_ok(hdr.hdr_version))
		goto cleanup;

	/* Verify the pack matches its index. */
	if (p->num_objects != ntohl(hdr.hdr_entries))
		goto cleanup;

	if (p_lseek(p->pack_fd, p->pack_size - GIT_OID_RAWSZ, SEEK_SET) == -1)
		goto cleanup;

	if (p_read(p->pack_fd, sha1.id, GIT_OID_RAWSZ) < GIT_SUCCESS)
		goto cleanup;

	idx_sha1 = ((unsigned char *)p->index_map.data) + p->index_map.len - 40;

	if (git_oid_cmp(&sha1, (git_oid *)idx_sha1) != 0)
		goto cleanup;

	return GIT_SUCCESS;

cleanup:
	p_close(p->pack_fd);
	p->pack_fd = -1;
	return git__throw(GIT_EPACKCORRUPTED, "Failed to packfile. Pack is corrupted");
}

static int packfile_check(struct pack_file **pack_out, const char *path)
{
	struct stat st;
	struct pack_file *p;
	size_t path_len;

	*pack_out = NULL;
	path_len = strlen(path);
	p = packfile_alloc(path_len + 2);

	/*
	 * Make sure a corresponding .pack file exists and that
	 * the index looks sane.
	 */
	path_len -= STRLEN(".idx");
	if (path_len < 1) {
		free(p);
		return git__throw(GIT_ENOTFOUND, "Failed to check packfile. Wrong path name");
	}

	memcpy(p->pack_name, path, path_len);

	strcpy(p->pack_name + path_len, ".keep");
	if (git_futils_exists(p->pack_name) == GIT_SUCCESS)
		p->pack_keep = 1;

	strcpy(p->pack_name + path_len, ".pack");
	if (p_stat(p->pack_name, &st) < GIT_SUCCESS || !S_ISREG(st.st_mode)) {
		free(p);
		return git__throw(GIT_ENOTFOUND, "Failed to check packfile. File not found");
	}

	/* ok, it looks sane as far as we can check without
	 * actually mapping the pack file.
	 */
	p->pack_size = (off_t)st.st_size;
	p->pack_local = 1;
	p->mtime = (git_time_t)st.st_mtime;

	/* see if we can parse the sha1 oid in the packfile name */
	if (path_len < 40 ||
		git_oid_fromstr(&p->sha1, path + path_len - GIT_OID_HEXSZ) < GIT_SUCCESS)
		memset(&p->sha1, 0x0, GIT_OID_RAWSZ);

	*pack_out = p;
	return GIT_SUCCESS;
}

static int packfile_load__cb(void *_data, char *path)
{
	struct pack_backend *backend = (struct pack_backend *)_data;
	struct pack_file *pack;
	int error;
	size_t i;

	if (git__suffixcmp(path, ".idx") != 0)
		return GIT_SUCCESS; /* not an index */

	for (i = 0; i < backend->packs.length; ++i) {
		struct pack_file *p = git_vector_get(&backend->packs, i);
		if (memcmp(p->pack_name, path, strlen(path) - STRLEN(".idx")) == 0)
			return GIT_SUCCESS;
	}

	error = packfile_check(&pack, path);
	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to load packfile");

	if (git_vector_insert(&backend->packs, pack) < GIT_SUCCESS) {
		free(pack);
		return GIT_ENOMEM;
	}

	return GIT_SUCCESS;
}

static int packfile_refresh_all(struct pack_backend *backend)
{
	int error;
	struct stat st;

	if (backend->pack_folder == NULL)
		return GIT_SUCCESS;

	if (p_stat(backend->pack_folder, &st) < 0 || !S_ISDIR(st.st_mode))
		return git__throw(GIT_ENOTFOUND, "Failed to refresh packfiles. Backend not found");

	if (st.st_mtime != backend->pack_folder_mtime) {
		char path[GIT_PATH_MAX];
		strcpy(path, backend->pack_folder);

		/* reload all packs */
		error = git_futils_direach(path, GIT_PATH_MAX, packfile_load__cb, (void *)backend);
		if (error < GIT_SUCCESS)
			return git__rethrow(error, "Failed to refresh packfiles");

		git_vector_sort(&backend->packs);
		backend->pack_folder_mtime = st.st_mtime;
	}

	return GIT_SUCCESS;
}








/***********************************************************
 *
 * PACKFILE ENTRY SEARCH INTERNALS
 *
 ***********************************************************/

static off_t nth_packed_object_offset(const struct pack_file *p, uint32_t n)
{
	const unsigned char *index = p->index_map.data;
	index += 4 * 256;
	if (p->index_version == 1) {
		return ntohl(*((uint32_t *)(index + 24 * n)));
	} else {
		uint32_t off;
		index += 8 + p->num_objects * (20 + 4);
		off = ntohl(*((uint32_t *)(index + 4 * n)));
		if (!(off & 0x80000000))
			return off;
		index += p->num_objects * 4 + (off & 0x7fffffff) * 8;
		return (((uint64_t)ntohl(*((uint32_t *)(index + 0)))) << 32) |
				   ntohl(*((uint32_t *)(index + 4)));
	}
}

static int pack_entry_find_offset(
		off_t *offset_out,
		git_oid *found_oid,
		struct pack_file *p,
		const git_oid *short_oid,
		unsigned int len)
{
	const uint32_t *level1_ofs = p->index_map.data;
	const unsigned char *index = p->index_map.data;
	unsigned hi, lo, stride;
	int pos, found = 0;
	const unsigned char *current = 0;

	*offset_out = 0;

	if (index == NULL) {
		int error;

		if ((error = pack_index_open(p)) < GIT_SUCCESS)
			return git__rethrow(error, "Failed to find offset for pack entry");

		assert(p->index_map.data);

		index = p->index_map.data;
		level1_ofs = p->index_map.data;
	}

	if (p->index_version > 1) {
		level1_ofs += 2;
		index += 8;
	}

	index += 4 * 256;
	hi = ntohl(level1_ofs[(int)short_oid->id[0]]);
	lo = ((short_oid->id[0] == 0x0) ? 0 : ntohl(level1_ofs[(int)short_oid->id[0] - 1]));

	if (p->index_version > 1) {
		stride = 20;
	} else {
		stride = 24;
		index += 4;
	}

#ifdef INDEX_DEBUG_LOOKUP
	printf("%02x%02x%02x... lo %u hi %u nr %d\n",
		short_oid->id[0], short_oid->id[1], short_oid->id[2], lo, hi, p->num_objects);
#endif

	/* Use git.git lookup code */
	pos =  sha1_entry_pos(index, stride, 0, lo, hi, p->num_objects, short_oid->id);

	if (pos >= 0) {
		/* An object matching exactly the oid was found */
		found = 1;
		current = index + pos * stride;
	} else {
		/* No object was found */
		/* pos refers to the object with the "closest" oid to short_oid */
		pos = - 1 - pos;
		if (pos < (int)p->num_objects) {
			current = index + pos * stride;

			if (!git_oid_ncmp(short_oid, (const git_oid *)current, len)) {
				found = 1;
			}
		}
	}

	if (found && pos + 1 < (int)p->num_objects) {
		/* Check for ambiguousity */
		const unsigned char *next = current + stride;

		if (!git_oid_ncmp(short_oid, (const git_oid *)next, len)) {
			found = 2;
		}
	}

	if (!found) {
		return git__throw(GIT_ENOTFOUND, "Failed to find offset for pack entry. Entry not found");
	} else if (found > 1) {
		return git__throw(GIT_EAMBIGUOUSOIDPREFIX, "Failed to find offset for pack entry. Ambiguous sha1 prefix within pack");
	} else {
		*offset_out = nth_packed_object_offset(p, pos);
		git_oid_fromraw(found_oid, current);

#ifdef INDEX_DEBUG_LOOKUP
		unsigned char hex_sha1[GIT_OID_HEXSZ + 1];
		git_oid_fmt(hex_sha1, found_oid);
		hex_sha1[GIT_OID_HEXSZ] = '\0';
		printf("found lo=%d %s\n", lo, hex_sha1);
#endif
		return GIT_SUCCESS;
	}
}

static int pack_entry_find1(
		struct pack_entry *e,
		struct pack_file *p,
		const git_oid *short_oid,
		unsigned int len)
{
	off_t offset;
	git_oid found_oid;
	int error;

	assert(p);

	if (len == GIT_OID_HEXSZ && p->num_bad_objects) {
		unsigned i;
		for (i = 0; i < p->num_bad_objects; i++)
			if (git_oid_cmp(short_oid, &p->bad_object_sha1[i]) == 0)
				return git__throw(GIT_ERROR, "Failed to find pack entry. Bad object found");
	}

	error = pack_entry_find_offset(&offset, &found_oid, p, short_oid, len);
	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to find pack entry. Couldn't find offset");

	/* we found a unique entry in the index;
	 * make sure the packfile backing the index
	 * still exists on disk */
	if (p->pack_fd == -1 && packfile_open(p) < GIT_SUCCESS)
		return git__throw(GIT_EOSERR, "Failed to find pack entry. Packfile doesn't exist on disk");

	e->offset = offset;
	e->p = p;

	git_oid_cpy(&e->sha1, &found_oid);
	return GIT_SUCCESS;
}

static int pack_entry_find(struct pack_entry *e, struct pack_backend *backend, const git_oid *oid)
{
	int error;
	size_t i;

	if ((error = packfile_refresh_all(backend)) < GIT_SUCCESS)
		return git__rethrow(error, "Failed to find pack entry");

	if (backend->last_found &&
		pack_entry_find1(e, backend->last_found, oid, GIT_OID_HEXSZ) == GIT_SUCCESS)
		return GIT_SUCCESS;

	for (i = 0; i < backend->packs.length; ++i) {
		struct pack_file *p;

		p = git_vector_get(&backend->packs, i);
		if (p == backend->last_found)
			continue;

		if (pack_entry_find1(e, p, oid, GIT_OID_HEXSZ) == GIT_SUCCESS) {
			backend->last_found = p;
			return GIT_SUCCESS;
		}
	}

	return git__throw(GIT_ENOTFOUND, "Failed to find pack entry");
}

static int pack_entry_find_prefix(
	struct pack_entry *e,
	struct pack_backend *backend,
	const git_oid *short_oid,
	unsigned int len)
{
	int error;
	size_t i;
	unsigned found = 0;

	if ((error = packfile_refresh_all(backend)) < GIT_SUCCESS)
		return git__rethrow(error, "Failed to find pack entry");

	if (backend->last_found) {
		error = pack_entry_find1(e, backend->last_found, short_oid, len);
		if (error == GIT_EAMBIGUOUSOIDPREFIX) {
			return git__rethrow(error, "Failed to find pack entry. Ambiguous sha1 prefix");
		} else if (error == GIT_SUCCESS) {
			found = 1;
		}
	}

	for (i = 0; i < backend->packs.length; ++i) {
		struct pack_file *p;

		p = git_vector_get(&backend->packs, i);
		if (p == backend->last_found)
			continue;

		error = pack_entry_find1(e, p, short_oid, len);
		if (error == GIT_EAMBIGUOUSOIDPREFIX) {
			return git__rethrow(error, "Failed to find pack entry. Ambiguous sha1 prefix");
		} else if (error == GIT_SUCCESS) {
			found++;
			if (found > 1)
				break;
			backend->last_found = p;
		}
	}

	if (!found) {
		return git__rethrow(GIT_ENOTFOUND, "Failed to find pack entry");
	} else if (found > 1) {
		return git__rethrow(GIT_EAMBIGUOUSOIDPREFIX, "Failed to find pack entry. Ambiguous sha1 prefix");
	} else {
		return GIT_SUCCESS;
	}

}












/***********************************************************
 *
 * PACKFILE ENTRY UNPACK INTERNALS
 *
 ***********************************************************/

static unsigned long packfile_unpack_header1(
		size_t *sizep,
		git_otype *type,
		const unsigned char *buf,
		unsigned long len)
{
	unsigned shift;
	unsigned long size, c;
	unsigned long used = 0;

	c = buf[used++];
	*type = (c >> 4) & 7;
	size = c & 15;
	shift = 4;
	while (c & 0x80) {
		if (len <= used || bitsizeof(long) <= shift)
			return 0;

		c = buf[used++];
		size += (c & 0x7f) << shift;
		shift += 7;
	}

	*sizep = (size_t)size;
	return used;
}

static int packfile_unpack_header(
		size_t *size_p,
		git_otype *type_p,
		struct pack_backend *backend,
		struct pack_file *p,
		struct pack_window **w_curs,
		off_t *curpos)
{
	unsigned char *base;
	unsigned int left;
	unsigned long used;

	/* pack_window_open() assures us we have [base, base + 20) available
	 * as a range that we can look at at.  (Its actually the hash
	 * size that is assured.)  With our object header encoding
	 * the maximum deflated object size is 2^137, which is just
	 * insane, so we know won't exceed what we have been given.
	 */
	base = pack_window_open(backend, p, w_curs, *curpos, &left);
	if (base == NULL)
		return GIT_ENOMEM;

	used = packfile_unpack_header1(size_p, type_p, base, left);

	if (used == 0)
		return git__throw(GIT_EOBJCORRUPTED, "Header length is zero");

	*curpos += used;
	return GIT_SUCCESS;
}

static int packfile_unpack_compressed(
		git_rawobj *obj,
		struct pack_backend *backend,
		struct pack_file *p,
		struct pack_window **w_curs,
		off_t curpos,
		size_t size,
		git_otype type)
{
	int st;
	z_stream stream;
	unsigned char *buffer, *in;

	buffer = git__malloc(size + 1);
	memset(buffer, 0x0, size + 1);

	memset(&stream, 0, sizeof(stream));
	stream.next_out = buffer;
	stream.avail_out = size + 1;

	st = inflateInit(&stream);
	if (st != Z_OK) {
		free(buffer);
		return git__throw(GIT_EZLIB, "Error in zlib");
	}

	do {
		in = pack_window_open(backend, p, w_curs, curpos, &stream.avail_in);
		stream.next_in = in;
		st = inflate(&stream, Z_FINISH);

		if (!stream.avail_out)
			break; /* the payload is larger than it should be */

		curpos += stream.next_in - in;
	} while (st == Z_OK || st == Z_BUF_ERROR);

	inflateEnd(&stream);

	if ((st != Z_STREAM_END) || stream.total_out != size) {
		free(buffer);
		return git__throw(GIT_EZLIB, "Error in zlib");
	}

	obj->type = type;
	obj->len = size;
	obj->data = buffer;
	return GIT_SUCCESS;
}

static off_t get_delta_base(
		struct pack_backend *backend,
		struct pack_file *p,
		struct pack_window **w_curs,
		off_t *curpos,
		git_otype type,
		off_t delta_obj_offset)
{
	unsigned char *base_info = pack_window_open(backend, p, w_curs, *curpos, NULL);
	off_t base_offset;
	git_oid unused;

	/* pack_window_open() assured us we have [base_info, base_info + 20)
	 * as a range that we can look at without walking off the
	 * end of the mapped window.  Its actually the hash size
	 * that is assured.  An OFS_DELTA longer than the hash size
	 * is stupid, as then a REF_DELTA would be smaller to store.
	 */
	if (type == GIT_OBJ_OFS_DELTA) {
		unsigned used = 0;
		unsigned char c = base_info[used++];
		base_offset = c & 127;
		while (c & 128) {
			base_offset += 1;
			if (!base_offset || MSB(base_offset, 7))
				return 0;  /* overflow */
			c = base_info[used++];
			base_offset = (base_offset << 7) + (c & 127);
		}
		base_offset = delta_obj_offset - base_offset;
		if (base_offset <= 0 || base_offset >= delta_obj_offset)
			return 0;  /* out of bound */
		*curpos += used;
	} else if (type == GIT_OBJ_REF_DELTA) {
		/* The base entry _must_ be in the same pack */
		if (pack_entry_find_offset(&base_offset, &unused, p, (git_oid *)base_info, GIT_OID_HEXSZ) < GIT_SUCCESS)
			return git__throw(GIT_EPACKCORRUPTED, "Base entry delta is not in the same pack");
		*curpos += 20;
	} else
		return 0;

	return base_offset;
}

static int packfile_unpack_delta(
		git_rawobj *obj,
		struct pack_backend *backend,
		struct pack_file *p,
		struct pack_window **w_curs,
		off_t curpos,
		size_t delta_size,
		git_otype delta_type,
		off_t obj_offset)
{
	off_t base_offset;
	git_rawobj base, delta;
	int error;

	base_offset = get_delta_base(backend, p, w_curs, &curpos, delta_type, obj_offset);
	if (base_offset == 0)
		return git__throw(GIT_EOBJCORRUPTED, "Delta offset is zero");

	pack_window_close(w_curs);
	error = packfile_unpack(&base, backend, p, base_offset);

	/* TODO: git.git tries to load the base from other packfiles
	 * or loose objects */
	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Corrupted delta");

	error = packfile_unpack_compressed(&delta, backend, p, w_curs, curpos, delta_size, delta_type);
	if (error < GIT_SUCCESS) {
		free(base.data);
		return git__rethrow(error, "Corrupted delta");
	}

	obj->type = base.type;
	error = git__delta_apply(obj,
			base.data, base.len,
			delta.data, delta.len);

	free(base.data);
	free(delta.data);

	/* TODO: we might want to cache this shit. eventually */
	//add_delta_base_cache(p, base_offset, base, base_size, *type);
	return error; /* error set by git__delta_apply */
}

static int packfile_unpack(
		git_rawobj *obj,
		struct pack_backend *backend,
		struct pack_file *p,
		off_t obj_offset)
{
	struct pack_window *w_curs = NULL;
	off_t curpos = obj_offset;
	int error;

	size_t size = 0;
	git_otype type;

	/*
	 * TODO: optionally check the CRC on the packfile
	 */

	obj->data = NULL;
	obj->len = 0;
	obj->type = GIT_OBJ_BAD;

	error = packfile_unpack_header(&size, &type, backend, p, &w_curs, &curpos);
	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to unpack packfile");

	switch (type) {
	case GIT_OBJ_OFS_DELTA:
	case GIT_OBJ_REF_DELTA:
		error = packfile_unpack_delta(
				obj, backend, p, &w_curs, curpos,
				size, type, obj_offset);
		break;

	case GIT_OBJ_COMMIT:
	case GIT_OBJ_TREE:
	case GIT_OBJ_BLOB:
	case GIT_OBJ_TAG:
		error = packfile_unpack_compressed(
				obj, backend, p, &w_curs, curpos,
				size, type);
		break;

	default:
		error = GIT_EOBJCORRUPTED;
		break;
	}

	pack_window_close(&w_curs);
	return error == GIT_SUCCESS ? GIT_SUCCESS : git__rethrow(error, "Failed to unpack packfile");
}





/***********************************************************
 *
 * PACKED BACKEND PUBLIC API
 *
 * Implement the git_odb_backend API calls
 *
 ***********************************************************/

/*
int pack_backend__read_header(git_rawobj *obj, git_odb_backend *backend, const git_oid *oid)
{
	pack_location location;

	assert(obj && backend && oid);

	if (locate_packfile(&location, (struct pack_backend *)backend, oid) < 0)
		return GIT_ENOTFOUND;

	return read_header_packed(obj, &location);
}
*/

int pack_backend__read(void **buffer_p, size_t *len_p, git_otype *type_p, git_odb_backend *backend, const git_oid *oid)
{
	struct pack_entry e;
	git_rawobj raw;
	int error;

	if ((error = pack_entry_find(&e, (struct pack_backend *)backend, oid)) < GIT_SUCCESS)
		return git__rethrow(error, "Failed to read pack backend");

	if ((error = packfile_unpack(&raw, (struct pack_backend *)backend, e.p, e.offset)) < GIT_SUCCESS)
		return git__rethrow(error, "Failed to read pack backend");

	*buffer_p = raw.data;
	*len_p = raw.len;
	*type_p = raw.type;

	return GIT_SUCCESS;
}

int pack_backend__read_prefix(
	git_oid *out_oid,
	void **buffer_p,
	size_t *len_p,
	git_otype *type_p,
	git_odb_backend *backend,
	const git_oid *short_oid,
	unsigned int len)
{
	if (len < GIT_OID_MINPREFIXLEN)
		return git__throw(GIT_EAMBIGUOUSOIDPREFIX, "Failed to read pack backend. Prefix length is lower than %d.", GIT_OID_MINPREFIXLEN);

	if (len >= GIT_OID_HEXSZ) {
		/* We can fall back to regular read method */
		int error = pack_backend__read(buffer_p, len_p, type_p, backend, short_oid);
		if (error == GIT_SUCCESS)
			git_oid_cpy(out_oid, short_oid);

		return error;
	} else {
		struct pack_entry e;
		git_rawobj raw;
		int error;

		if ((error = pack_entry_find_prefix(&e, (struct pack_backend *)backend, short_oid, len)) < GIT_SUCCESS)
			return git__rethrow(error, "Failed to read pack backend");

		if ((error = packfile_unpack(&raw, (struct pack_backend *)backend, e.p, e.offset)) < GIT_SUCCESS)
			return git__rethrow(error, "Failed to read pack backend");

		*buffer_p = raw.data;
		*len_p = raw.len;
		*type_p = raw.type;
		git_oid_cpy(out_oid, &e.sha1);
	}

	return GIT_SUCCESS;
}

int pack_backend__exists(git_odb_backend *backend, const git_oid *oid)
{
	struct pack_entry e;
	return pack_entry_find(&e, (struct pack_backend *)backend, oid) == GIT_SUCCESS;
}

void pack_backend__free(git_odb_backend *_backend)
{
	struct pack_backend *backend;
	size_t i;

	assert(_backend);

	backend = (struct pack_backend *)_backend;

	for (i = 0; i < backend->packs.length; ++i) {
		struct pack_file *p = git_vector_get(&backend->packs, i);
		packfile_free(backend, p);
	}

	git_vector_free(&backend->packs);
	free(backend->pack_folder);
	free(backend);
}

int git_odb_backend_pack(git_odb_backend **backend_out, const char *objects_dir)
{
	struct pack_backend *backend;
	char path[GIT_PATH_MAX];

	backend = git__calloc(1, sizeof(struct pack_backend));
	if (backend == NULL)
		return GIT_ENOMEM;

	if (git_vector_init(&backend->packs, 8, packfile_sort__cb) < GIT_SUCCESS) {
		free(backend);
		return GIT_ENOMEM;
	}

	backend->window_size = DEFAULT_WINDOW_SIZE;
	backend->mapped_limit = DEFAULT_MAPPED_LIMIT;

	git_path_join(path, objects_dir, "pack");
	if (git_futils_isdir(path) == GIT_SUCCESS) {
		backend->pack_folder = git__strdup(path);
		backend->pack_folder_mtime = 0;

		if (backend->pack_folder == NULL) {
			free(backend);
			return GIT_ENOMEM;
		}
	}

	backend->parent.read = &pack_backend__read;
	backend->parent.read_prefix = &pack_backend__read_prefix;
	backend->parent.read_header = NULL;
	backend->parent.exists = &pack_backend__exists;
	backend->parent.free = &pack_backend__free;

	*backend_out = (git_odb_backend *)backend;
	return GIT_SUCCESS;
}
