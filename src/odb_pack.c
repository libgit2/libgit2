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
#include "fileops.h"
#include "hash.h"
#include "odb.h"
#include "delta-apply.h"

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

struct pack__dirent {
	struct pack_backend *backend;
	int is_pack_local;
};

struct pack_backend {
	git_odb_backend parent;
	git_vector packs;
	struct pack_file *last_found;

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
static int packfile_check(struct pack_file **pack_out, const char *path, int local);
static int packfile_load__cb(void *_data, char *path);
static int packfile_load_all(struct pack_backend *backend, const char *odb_path, int local);

static off_t nth_packed_object_offset(const struct pack_file *p, uint32_t n);

static int pack_entry_find_offset(off_t *offset_out,
		struct pack_file *p, const git_oid *oid);

static int pack_entry_find1(struct pack_entry *e,
		struct pack_file *p, const git_oid *oid);

static int pack_entry_find(struct pack_entry *e,
		struct pack_backend *backend, const git_oid *oid);

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
		git_otype type,
		size_t *packedSize);

static int packfile_unpack_delta(
		git_rawobj *obj,
		struct pack_backend *backend,
		struct pack_file *p,
		struct pack_window **w_curs,
		off_t curpos,
		size_t delta_size,
		git_otype delta_type,
		off_t obj_offset,
		size_t *packedSize);

static int packfile_unpack(git_rawobj *obj, struct pack_backend *backend,
		struct pack_file *p, off_t obj_offset, size_t *packedSize);





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

		gitfo_free_map(&w->window_map);

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
		gitfo_free_map(&lru_w->window_map);

		if (lru_l)
			lru_l->next = lru_w->next;
		else {
			lru_p->windows = lru_w->next;
			if (!lru_p->windows && lru_p->pack_fd != keep_fd) {
				gitfo_close(lru_p->pack_fd);
				lru_p->pack_fd = -1;
			}
		}

		free(lru_w);
		backend->open_windows--;
		return GIT_SUCCESS;
	}

	return GIT_ERROR;
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

			if (gitfo_map_ro(&win->window_map, p->pack_fd,
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
		gitfo_free_map(&p->index_map);
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
	git_file fd = gitfo_open(path, O_RDONLY /*| O_NOATIME */);

	int error;

	if (fd < 0)
		return GIT_EOSERR;

	if (gitfo_fstat(fd, &st) < GIT_SUCCESS) {
		gitfo_close(fd);
		return GIT_EOSERR;
	}

	if (!git__is_sizet(st.st_size))
		return GIT_ENOMEM;

	idx_size = (size_t)st.st_size;

	if (idx_size < 4 * 256 + 20 + 20) {
		gitfo_close(fd);
		return GIT_EOBJCORRUPTED;
	}

	error = gitfo_map_ro(&p->index_map, fd, 0, idx_size);
	gitfo_close(fd);

	if (error < GIT_SUCCESS)
		return error;

	hdr = idx_map = p->index_map.data;

	if (hdr->idx_signature == htonl(PACK_IDX_SIGNATURE)) {
		version = ntohl(hdr->idx_version);

		if (version < 2 || version > 2) {
			gitfo_free_map(&p->index_map);
			return GIT_EOBJCORRUPTED; /* unsupported index version */
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
			gitfo_free_map(&p->index_map);
			return GIT_EOBJCORRUPTED; /* non-monotonic index */
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
			gitfo_free_map(&p->index_map);
			return GIT_EOBJCORRUPTED;
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
			gitfo_free_map(&p->index_map);
			return GIT_EOBJCORRUPTED;
		}

		/* Make sure that off_t is big enough to access the whole pack...
		 * Is this an issue in libgit2? It shouldn't. */
		if (idx_size != min_size && (sizeof(off_t) <= 4)) {
			gitfo_free_map(&p->index_map);
			return GIT_EOSERR;
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

	return error;
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
		gitfo_close(p->pack_fd);

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
		return GIT_ENOTFOUND;

	/* TODO: open with noatime */
	p->pack_fd = gitfo_open(p->pack_name, O_RDONLY);
	if (p->pack_fd < 0 || gitfo_fstat(p->pack_fd, &st) < GIT_SUCCESS)
		return GIT_EOSERR;

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
	if (gitfo_read(p->pack_fd, &hdr, sizeof(hdr)) < GIT_SUCCESS)
		goto cleanup;

	if (hdr.hdr_signature != htonl(PACK_SIGNATURE))
		goto cleanup;

	if (!pack_version_ok(hdr.hdr_version))
		goto cleanup;

	/* Verify the pack matches its index. */
	if (p->num_objects != ntohl(hdr.hdr_entries))
		goto cleanup;

	if (gitfo_lseek(p->pack_fd, p->pack_size - GIT_OID_RAWSZ, SEEK_SET) == -1)
		goto cleanup;

	if (gitfo_read(p->pack_fd, sha1.id, GIT_OID_RAWSZ) < GIT_SUCCESS)
		goto cleanup;

	idx_sha1 = ((unsigned char *)p->index_map.data) + p->index_map.len - 40;

	if (git_oid_cmp(&sha1, (git_oid *)idx_sha1) != 0)
		goto cleanup;

	return GIT_SUCCESS;	

cleanup:
	gitfo_close(p->pack_fd);
	p->pack_fd = -1;
	return GIT_EPACKCORRUPTED;
}

static int packfile_check(struct pack_file **pack_out, const char *path, int local)
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
		return GIT_ENOTFOUND;
	}

	memcpy(p->pack_name, path, path_len);

	strcpy(p->pack_name + path_len, ".keep");
	if (gitfo_exists(p->pack_name) == GIT_SUCCESS)
		p->pack_keep = 1;

	strcpy(p->pack_name + path_len, ".pack");
	if (gitfo_stat(p->pack_name, &st) < GIT_SUCCESS || !S_ISREG(st.st_mode)) {
		free(p);
		return GIT_ENOTFOUND;
	}

	/* ok, it looks sane as far as we can check without
	 * actually mapping the pack file.
	 */
	p->pack_size = (off_t)st.st_size;
	p->pack_local = local;
	p->mtime = (git_time_t)st.st_mtime;

	/* see if we can parse the sha1 oid in the packfile name */
	if (path_len < 40 ||
		git_oid_mkstr(&p->sha1, path + path_len - GIT_OID_HEXSZ) < GIT_SUCCESS)
		memset(&p->sha1, 0x0, GIT_OID_RAWSZ);

	*pack_out = p;
	return GIT_SUCCESS;
}

static int packfile_load__cb(void *_data, char *path)
{
	struct pack__dirent *data = (struct pack__dirent *)_data;
	struct pack_file *pack;
	int error;

	if (git__suffixcmp(path, ".idx") != 0)
		return GIT_SUCCESS; /* not an index */

	/* FIXME: git.git checks for duplicate packs.
	 * But that makes no fucking sense. Our dirent is not
	 * going to generate dupicate entries */

	error = packfile_check(&pack, path, data->is_pack_local);
	if (error < GIT_SUCCESS)
		return error;

	if (git_vector_insert(&data->backend->packs, pack) < GIT_SUCCESS) {
		free(pack);
		return GIT_ENOMEM;
	}

	return GIT_SUCCESS;
}

static int packfile_load_all(struct pack_backend *backend, const char *odb_path, int local)
{
	int error;
	char path[GIT_PATH_MAX];
	struct pack__dirent data;

	data.backend = backend;
	data.is_pack_local = local;

	git__joinpath(path, odb_path, "pack");
	if (gitfo_isdir(path) < GIT_SUCCESS)
		return GIT_SUCCESS;

	error = gitfo_dirent(path, GIT_PATH_MAX, packfile_load__cb, (void *)&data);
	if (error < GIT_SUCCESS)
		return error;

	git_vector_sort(&backend->packs);
	backend->last_found = git_vector_get(&backend->packs, 0);

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
		struct pack_file *p,
		const git_oid *oid)
{
	const uint32_t *level1_ofs = p->index_map.data;
	const unsigned char *index = p->index_map.data;
	unsigned hi, lo, stride;

	*offset_out = 0;

	if (index == NULL) {
		int error;

		if ((error = pack_index_open(p)) < GIT_SUCCESS)
			return error;

		assert(p->index_map.data);

		index = p->index_map.data;
		level1_ofs = p->index_map.data;
	}

	if (p->index_version > 1) {
		level1_ofs += 2;
		index += 8;
	}

	index += 4 * 256;
	hi = ntohl(level1_ofs[(int)oid->id[0]]);
	lo = ((oid->id[0] == 0x0) ? 0 : ntohl(level1_ofs[(int)oid->id[0] - 1]));

	if (p->index_version > 1) {
		stride = 20;
	} else {
		stride = 24;
		index += 4;
	}

#ifdef INDEX_DEBUG_LOOKUP
	printf("%02x%02x%02x... lo %u hi %u nr %d\n",
		oid->id[0], oid->id[1], oid->id[2], lo, hi, p->num_objects);
#endif

#ifdef GIT2_INDEX_LOOKUP /* TODO: use the advanced lookup method from git.git */

	int pos = sha1_entry_pos(index, stride, 0, lo, hi, p->num_objects, oid);
	if (pos < 0)
		return GIT_ENOTFOUND;

	*offset_out = nth_packed_object_offset(p, pos);
	return GIT_SUCCESS;

#else /* use an old and boring binary search */

	do {
		unsigned mi = (lo + hi) / 2;
		int cmp = memcmp(index + mi * stride, oid->id, GIT_OID_RAWSZ);

		if (!cmp) {
			*offset_out = nth_packed_object_offset(p, mi);
			return GIT_SUCCESS;
		}

		if (cmp > 0)
			hi = mi;
		else
			lo = mi+1;

	} while (lo < hi);

	return GIT_ENOTFOUND;
#endif
}

static int pack_entry_find1(
		struct pack_entry *e,
		struct pack_file *p,
		const git_oid *oid)
{
	off_t offset;

	assert(p);

	if (p->num_bad_objects) {
		unsigned i;
		for (i = 0; i < p->num_bad_objects; i++)
			if (git_oid_cmp(oid, &p->bad_object_sha1[i]) == 0)
				return GIT_ERROR;
	}

	if (pack_entry_find_offset(&offset, p, oid) < GIT_SUCCESS)
		return GIT_ENOTFOUND;
	
	/* we found an entry in the index;
	 * make sure the packfile backing the index 
	 * still exists on disk */
	if (p->pack_fd == -1 && packfile_open(p) < GIT_SUCCESS)
		return GIT_EOSERR;

	e->offset = offset;
	e->p = p;

	git_oid_cpy(&e->sha1, oid);
	return GIT_SUCCESS;
}

static int pack_entry_find(struct pack_entry *e, struct pack_backend *backend, const git_oid *oid)
{
	size_t i;

	if (backend->last_found &&
		pack_entry_find1(e, backend->last_found, oid) == GIT_SUCCESS)
		return GIT_SUCCESS;

	for (i = 0; i < backend->packs.length; ++i) {
		struct pack_file *p;

		p = git_vector_get(&backend->packs, i);
		if (p == backend->last_found)
			continue;

		if (pack_entry_find1(e, p, oid) == GIT_SUCCESS) {
			backend->last_found = p;
			return GIT_SUCCESS;
		}
	}

	return GIT_ENOTFOUND;
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
		return GIT_EOBJCORRUPTED;

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
		git_otype type,
		size_t *dataSize)
{
	int st;
	z_stream stream;
	unsigned char *buffer, *in;
	off_t originalPos = curpos;

	buffer = git__malloc(size);

	memset(&stream, 0, sizeof(stream));
	stream.next_out = buffer;
	stream.avail_out = size + 1;

	st = inflateInit(&stream);
	if (st != Z_OK) {
		free(buffer);
		return GIT_EZLIB;
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
		return GIT_EZLIB;
	}

	if(dataSize) {
		*dataSize = curpos - originalPos;
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
	int error;

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
		*curpos += 20;

		/* The base entry _must_ be in the same pack */
		error = pack_entry_find_offset(&base_offset, p, (git_oid *)base_info);
		if (error < GIT_SUCCESS)
			return error;
	} else
		return GIT_EPACKCORRUPTED;

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
		off_t obj_offset,
		size_t *packedSize)
{
	off_t base_offset;
	off_t originalOffset = curpos;
	git_rawobj base, delta;
	int error;
	size_t compressedSize;

	base_offset = get_delta_base(backend, p, w_curs, &curpos, delta_type, obj_offset);

	error = packfile_unpack_compressed(&delta, backend, p, w_curs, curpos, delta_size, delta_type, &compressedSize);
	if (error < GIT_SUCCESS) {
		return error;
	}

	// If we're asked for packed size, we'll feed it now, as the base resolution
	// might have failed.
	if((base_offset > GIT_SUCCESS || base_offset == GIT_ENOTFOUND) && packedSize) {
		// curpos - originalOffset will be equal to however large the delta preamble is
		// (either 20 bytes for ref delta, or a random amount of bytes for ofs delta).
		*packedSize = (curpos - originalOffset) + compressedSize;
	}

	if (base_offset < GIT_SUCCESS) {
		free(delta.data);
		return base_offset;
	}

	pack_window_close(w_curs);
	error = packfile_unpack(&base, backend, p, base_offset, NULL);

	/* TODO: git.git tries to load the base from other packfiles
	 * or loose objects */
	if (error < GIT_SUCCESS)
		return error;


	obj->type = base.type;
	error = git__delta_apply(obj,
			base.data, base.len,
			delta.data, delta.len);

	free(base.data);
	free(delta.data);

	/* TODO: we might want to cache this shit. eventually */
	//add_delta_base_cache(p, base_offset, base, base_size, *type);
	return error;
}

static int packfile_unpack(
		git_rawobj *obj,
		struct pack_backend *backend,
		struct pack_file *p,
		off_t obj_offset,
		size_t *packedSize)
{
	struct pack_window *w_curs = NULL;
	off_t curpos = obj_offset;
	int error;

	size_t size;
	git_otype type;
	size_t dataSize;

	/* 
	 * TODO: optionally check the CRC on the packfile
	 */

	obj->data = NULL;
	obj->len = 0;
	obj->type = GIT_OBJ_BAD;

	error = packfile_unpack_header(&size, &type, backend, p, &w_curs, &curpos);
	if (error < GIT_SUCCESS)
		return error;

	switch (type) {
	case GIT_OBJ_OFS_DELTA:
	case GIT_OBJ_REF_DELTA:
		error = packfile_unpack_delta(
				obj, backend, p, &w_curs, curpos,
				size, type, obj_offset, &dataSize);
		break;

	case GIT_OBJ_COMMIT:
	case GIT_OBJ_TREE:
	case GIT_OBJ_BLOB:
	case GIT_OBJ_TAG:
		error = packfile_unpack_compressed(
				obj, backend, p, &w_curs, curpos,
				size, type, &dataSize);
		break;

	default:
		error = GIT_EOBJCORRUPTED;
		break;
	}

	// The packed size is simply the size of the header plus the size of the data
	// compressed. We get header size like so:
	// curpos - obj_offset;
	// We only provide packed size if we didn't fail miserably.
	if((error == GIT_SUCCESS || error == GIT_ENOTFOUND) && packedSize) {
		*packedSize = (curpos - obj_offset) + dataSize;
	}

	pack_window_close(&w_curs);
	return error;
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
		return error;

	if ((error = packfile_unpack(&raw, (struct pack_backend *)backend, e.p, e.offset, NULL)) < GIT_SUCCESS)
		return error;

	*buffer_p = raw.data;
	*len_p = raw.len;
	*type_p = raw.type;

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
	free(backend);
}

int git_odb_backend_pack(git_odb_backend **backend_out, const char *objects_dir)
{
	int error;
	struct pack_backend *backend;

	backend = git__calloc(1, sizeof(struct pack_backend));
	if (backend == NULL)
		return GIT_ENOMEM;

	if (git_vector_init(&backend->packs, 8, packfile_sort__cb) < GIT_SUCCESS) {
		free(backend);
		return GIT_ENOMEM;
	}

	backend->window_size = DEFAULT_WINDOW_SIZE;
	backend->mapped_limit = DEFAULT_MAPPED_LIMIT;

	error = packfile_load_all(backend, objects_dir, 1);
	if (error < GIT_SUCCESS) {
		pack_backend__free((git_odb_backend *)backend);
		return error;
	}

	backend->parent.read = &pack_backend__read;
	backend->parent.read_header = NULL;
	backend->parent.exists = &pack_backend__exists;
	backend->parent.free = &pack_backend__free;

	*backend_out = (git_odb_backend *)backend;
	return GIT_SUCCESS;
}

/**
 * Chapter 4: The story after the never ending story.
 * --------------------------------------------------------
 *
 * Mark my words, all ye who dare meander past this point of no return! Beware!
 * Beyond this modest warning lies a wasteland of despair, tragedy, betrayal and
 * pure hatred that belies any label or classification. This is the stuff
 * nightmares are made of.
 *
 * Our story here embarks with our Packfile, who lost its companion, Index,
 * during The Great Packfile Transport Over The Great Network Divide. Packfile
 * trudges a lonely desert, unloved by the Libgit2 kingdom, whom will only admit
 * Packfile if he has his trusty Index companion.
 *
 * Packfile does not understand why the kingdom of Libgit2 will open their gates
 * only to same-sex partnerships for his kind, however this musing does not bear
 * much thought for Packfile, as every time he thinks about his long lost Index
 * life-partner, he chokes back tears of regret. Too often does his mind wander
 * back to that day. That fateful day during the Great Transport, when he and
 * Index had set off with such high hopes of a new land with great promise.
 * Packfile shrugs this thought away and focuses on putting one foot in front of
 * another, all the while contemplating the meaning of life. Who is he, without
 * his Index, but an empty shell of binary data? He couldn't even classify his
 * data with SHA1's without his dear Index. Packfile felt so lost inside.
 * Literally, he thought to himself with a smirk.
 *
 * Presently Packfile wandered into a nondescript hovel, nothing more than a
 * worn weatherbeaten canvas stretched between some twigs. The floor of the
 * hovel, he noted, were lined with the husks of old Commits and Trees from a
 * by-gone era.
 *
 * Hearing a stir behind him, Packfile spins around and nearly retches with the
 * sight he is greeted with. Indeed, standing before him is a foul crone, flesh
 * rotting, beady eyes darting over his body. With a grimace he notes a tiny
 * fleshy tongue flicks across its lips while it sizes up its quarry.
 *
 * "I sense you have lost someone important to you", bellows the old crone, with
 * nary a word of greeting or introduction. Stunned by this sudden revelation by
 * a complete stranger, Packfile murmurs a response. "That I have. My dear
 * friend Index".
 *
 * "Ahhh, but Index is not lost. His body may be committed to the Twisting Ether,
 * but his essence is here with us.", the crone replies. Continuing on she says
 * "I can bring him back to you, for a price.".
 *
 * Scarcely believing his ears, but not questioning his providence, Packfile
 * immediately responds. "I will pay any price you require."
 *
 * "A dance!", the crone cackles gleefully.
 *
 * "A what?" Packfile asks in confusion that is already borderlining on
 * annoyance. Who is this old crone to play silly gameS?
 *
 * "Never mind, we can skip the dance. Let us begin!", the crone exclaims.
 * Before Packfile even has a chance to begin, the crone closes the gap between
 * them and latches onto his body. Her tongue darts out and moves over his body,
 * and as she does so, he feels a sudden surge course throughout his being. He
 * suddenly becomes aware that his body is glowing, exposing the segments of
 * binary data he contains. Small luminescent cracks form all over him, suddenly
 * displaying the data boundaries for all to see.
 *
 * The crone suddenly lets go of him, and begins dancing around him maniacally.
 * At this point Packfile double-takes and rubs his eyes. Indeed, every time the
 * crone dances around him, he sees different parts of his body emitting a
 * spark. These sparks then dance around before him and settle on the ground. No
 * sooner has this strange happening occurred, the spark fades and turns to a
 * small black shard, settling on top of other shards.
 *
 * The crone continues her dance of insanity around Packfile, and as she does so
 * the shards begin to take shape. Packfile realizes with a start that this form
 * taking shape before him could be none other than his dear Index! In no time
 * at all, the crone completes her dance, and a lifeless Index statue is before
 * him.
 *
 * The crones attention shifts to the statue of Index, and she begins waving her
 * hands and chanting a strange tongue. Suddenly, Index springs to life. Smiles
 * and embraces are shared. Blah blah etc etc.
 *
 * Before Packfile leaves for the kingdom of Libgit2, reunited at last with his
 * dear Index, he turns to the crone and asks, "But what is your name?".
 *
 * To which the crone replies, "Some know me as git_pack_build_index".
 *
 * THE END.
 *
 * Here's how git_pack_build_index does its shit:
 * 1 Open a packfile and parse the header, validating the PACK preamble and
 *   ensuring the version is correct. It then saves off the entry count.
 * 2 Next it allocates a memory segment large enough to house a v2 index for the
 *   packfile and its contents.
 * 3 With this done, it will then begin parsing the packfile, resolving objects.
 * 4 If it can resolve an object (either it's a base object, or a delta with a
 *   base we've encountered before), it will hash the object and then save the
 *   offset + sha1 in a vector
 * 5 After it's gone thruogh the packfile, it will sort the vector of sha1s it
 *   has, and build as much of the index it can. Then it will start from #3 again.
 * 6 Once all objects have been resolved, the function will then go through and
 *   do final steps, which include hashing the index/packfile etc.
 * 7 The index will then be saved and success returned!
 *
 * TODO: calculate the CRC for each object.
 *
 */
static int packentries_sort_cb(const void *a_, const void *b_)
{
	struct pack_entry *a = *((struct pack_entry **)a_);
	struct pack_entry *b = *((struct pack_entry **)b_);

	// Sort by the sha1 yo.
	return git_oid_cmp(&a->sha1, &b->sha1);
}

int git_pack_build_index(const char *path)
{
	struct pack_file *p;
	size_t path_len;
	struct stat st;
	struct pack_backend *backend;
	struct pack_window *w_curs;
	unsigned char *data;
	struct pack_header *hdr;
	unsigned int remaining;
	uint32_t i;
	int32_t j;
	git_rawobj obj;
	size_t objPackSize;
	off_t offset;
	uint32_t *indexOffsets;
	uint32_t *indexFanout;
	git_oid *indexShas;
	char objHdr[64];
	int  objHdrlen;
	git_oid objId;
	int error;
	git_vector packEntries;
	size_t unresolvedObjectCount;
	size_t prev_unresolvedObjectCount = 0;
	char *resolvedObjects;
	uint32_t *resolvedSizes;
	struct pack_entry *tempEntry;
	short fanout;
	uint32_t fanout_offset;
	git_hash_ctx *hashCtx;
	git_oid indexSha1;

	// We'll be using a dummy pack_backend during this operation.
	backend = git__calloc(1, sizeof(struct pack_backend));
	if (backend == NULL)
		return GIT_ENOMEM;

	if (git_vector_init(&backend->packs, 1, packfile_sort__cb) < GIT_SUCCESS) {
		free(backend);
		return GIT_ENOMEM;
	}

	path_len = strlen(path);
	p = packfile_alloc(path_len + 2);
	strcpy(p->pack_name, path);

	if (gitfo_stat(p->pack_name, &st) < GIT_SUCCESS || !S_ISREG(st.st_mode)) {
		free(p);
		free(backend);
		return GIT_ENOTFOUND;
	}

	/* ok, it looks sane as far as we can check without
	 * actually mapping the pack file.
	 */
	p->pack_size = (off_t)st.st_size;
	p->pack_local = 1;
	p->mtime = (git_time_t)st.st_mtime;

	if(git_vector_insert(&backend->packs, p) < GIT_SUCCESS) {
		free(p);
		free(backend);
		return GIT_ENOMEM;
	}

	backend->window_size = DEFAULT_WINDOW_SIZE;
	backend->mapped_limit = DEFAULT_MAPPED_LIMIT;

	p->pack_fd = gitfo_open(p->pack_name, O_RDONLY);
	if (p->pack_fd < 0 || gitfo_fstat(p->pack_fd, &st) < GIT_SUCCESS) {
		free(p);
		free(backend);
		return GIT_EOSERR;
	}

	w_curs = NULL;
	data = pack_window_open(backend, p, &w_curs, 0, &remaining);
	if(data == NULL) {
		free(p);
		free(backend);
		return GIT_EOSERR;
	}

	assert(remaining > sizeof(struct pack_header));

	hdr = (struct pack_header*)data;

	if (hdr->hdr_signature != htonl(PACK_SIGNATURE)) {
		pack_window_close(&w_curs);
		free(p);
		free(backend);
		return GIT_EPACKCORRUPTED;
	}

	if (!pack_version_ok(hdr->hdr_version)) {
		pack_window_close(&w_curs);
		free(p);
		free(backend);
		return GIT_EPACKCORRUPTED;
	}

	// Save off entry count.
	p->num_objects = ntohl(hdr->hdr_entries);

	pack_window_close(&w_curs);

	// We'll be building an index file while we burn through the packfile itself.
	// The advantage with this is the internal pack implementation currently
	// relies on this index when it resolves ref deltas, so this way we'll not have
	// change the implementation much, we'll just work with it!

	// Calculating the size works like this:
	// > Header: 8 bytes
	// > Fanout, 256 * 4 bytes
	// > SHA list - entryCount * 20 bytes
	// > crc checksums - entryCount * 4 bytes
	// > packfile offset - entryCount * 4 bytes
	// > 64b_offsets currently unimplemented
	// > Trailer - 2 * 20 bytes
	p->index_version = 2;

	p->index_map.len = 8 +
			(256 * 4) + // fanout
			(p->num_objects * 20) + // sha1
			(p->num_objects * 4) + //crc
			(p->num_objects * 4) + // packfile offsets
			(0) + 	// 64b_offsets.
			(2 * 20); // trailers.

	p->index_map.data = git__malloc(p->index_map.len);
	memset(p->index_map.data, 0, p->index_map.len);

	// What we'll do is build a "dirty" index first - this index will just have
	// entries thrown into it as they're parsed (so the internal implementation
	// can resolve deltas), then we'll build a nice binary sorted one afterwards,
	// with fanout and crcs accounted for also. In order to do this though, the
	// entries we *do* have need to be sorted nicely otherwise binary search will
	// fail.
	git_vector_init(&packEntries, p->num_objects, packentries_sort_cb);

	// Let's setup some quick shortcuts to relevant parts of the index.
	indexOffsets = p->index_map.data + 8 + (256*4) + (p->num_objects*20) + (p->num_objects*4);
	indexShas = p->index_map.data + 8 + (256*4);
	indexFanout = p->index_map.data + 8;

	assert(p->num_objects > 0);

	// So the way we do this is like this:
	// We start looping, and collect all base objects from the pack, saving their
	// offsets and sha1s as we go on our merry way. With this done, we then sort
	// through the list again, looking for deltas that reference base objects we
	// have already parsed. We do this until all objects are accounted for, then
	// we write out the index. It needs to constantly loop because deltas can be
	// based off other deltas.
	// Each loop through we make sure we sort the entries we have so far nicely
	// by their sha1, then reconfigure the fanout list in the index. We do all of
	// this so that the internal implementation doesn't hate on us. It's a little
	// inefficient, but this indexing operation doesn't need to be a speed demon.
	// I didn't want to go through and refucktor the pack stuff for two reasons:
	// 1. I'm afraid
	// 2. ... I'm really afraid.

	// We use this as a quick reference for whether or not a given pack entry has
	// been resolved or not.
	resolvedObjects = git__calloc(p->num_objects, sizeof(char));
	resolvedSizes = git__calloc(p->num_objects, sizeof(uint32_t));
	memset(resolvedObjects, 0, p->num_objects);
	memset(resolvedSizes, 0, sizeof(uint32_t)*p->num_objects);

	unresolvedObjectCount = 0;

	do {
		if(prev_unresolvedObjectCount > 0 && (prev_unresolvedObjectCount == unresolvedObjectCount)) {
			error = GIT_EPACKCORRUPTED;
			goto cleanup;
		}
		prev_unresolvedObjectCount = unresolvedObjectCount;

		unresolvedObjectCount = 0;

		// Start from the top.
		offset = sizeof(struct pack_header);

		for(i = 0; i < p->num_objects; i++) {

			if(resolvedObjects[i]) {
				offset += resolvedSizes[i];
				continue;
			}

			error = packfile_unpack(&obj, backend, p, offset, &objPackSize);
			if(error < GIT_SUCCESS && error != GIT_ENOTFOUND) {
				error = GIT_EPACKCORRUPTED;
				goto cleanup;
			}

			if(error == GIT_SUCCESS) {
				if(git_odb__hash_obj(&objId, objHdr, sizeof(objHdr), &objHdrlen, &obj) < GIT_SUCCESS) {
					error = GIT_EPACKCORRUPTED;
					goto cleanup;
				}

				tempEntry = git__malloc(sizeof(struct pack_entry));
				tempEntry->offset = offset;
				tempEntry->p = p;
				tempEntry->sha1 = objId;
				git_vector_insert(&packEntries, tempEntry);

				resolvedObjects[i] = 1;
				resolvedSizes[i] = objPackSize;

				free(obj.data);
				char idStrLol[GIT_OID_HEXSZ];
				git_oid_fmt(idStrLol, &objId);
			}
			else {
				unresolvedObjectCount++;
			}

			offset += objPackSize;
		}

		// Right. That's an iteration done. We may or may not have all objects
		// accounted for. Either way we wanna sort out sha1 list nicely.
		git_vector_sort(&packEntries);

		// Now we'll write what we have for an index so far.
		fanout = 0;
		fanout_offset = 0;
		memset(indexFanout, 0, (255*4));
		memset(indexShas, 0, (GIT_OID_RAWSZ * p->num_objects));
		memset(indexOffsets, 0, (4 * p->num_objects));
		for(i = 0; i < packEntries.length; i++) {
			tempEntry = git_vector_get(&packEntries, i);

			indexOffsets[i] = htonl(tempEntry->offset);
			indexShas[i] = tempEntry->sha1;

			//fanout_firstOffset++;

			if(tempEntry->sha1.id[0] > fanout) {
				if(fanout > -1) {
					for(j = fanout; j < tempEntry->sha1.id[0]; j++) {
						indexFanout[j] = htonl(i);
					}
				}

				fanout = tempEntry->sha1.id[0];
			}

		}

		// Complete the fanout table.
		for(j = fanout; j <= 255; j++) {
			indexFanout[j] = htonl(i);
		}
	} while(unresolvedObjectCount > 0);

	// Right! If we got here we successfully resolved all objects. Good stuff.
	// Last steps are to generate crcs for all the objects, and then hash the
	// whole pack, then the whole index.


	hashCtx = git_hash_new_ctx();
	if(!hashCtx) {
		// Haha. What a shitty place to fail. I hope this never happens to anyone.
		error = GIT_ENOMEM;
		goto cleanup;
	}

	for(i = 0; i < packEntries.length; i++) {
		tempEntry = git_vector_get(&packEntries, i);
		git_hash_update(hashCtx, &tempEntry->sha1, GIT_OID_RAWSZ);
	}

	git_hash_final(&indexSha1, hashCtx);
	git_hash_free_ctx(hashCtx);

	error = GIT_SUCCESS;

cleanup:
fflush(NULL);
	for(j = packEntries.length - 1; j >= 0; j--) {
		free(git_vector_get(&packEntries, j));
	}
	git_vector_free(&packEntries);

	free(resolvedSizes);
	free(resolvedObjects);
	free(p->index_map.data);
	free(p);
	free(backend);
	return error;
}

