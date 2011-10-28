/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
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
#include "mwindow.h"
#include "pack.h"

#include "git2/odb_backend.h"

struct pack_backend {
	git_odb_backend parent;
	git_vector packs;
	struct git_pack_file *last_found;
	char *pack_folder;
	time_t pack_folder_mtime;
};

/**
 * The wonderful tale of a Packed Object lookup query
 * ===================================================
 *	A riveting and epic story of epicness and ASCII
 *			art, presented by yours truly,
 *				Sir Vicent of Marti
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
 * |
 *	|-# packfile_load_all
 *	 | Tries to find the `pack` folder, if it exists. ODBs without
 *	 | a pack folder are ignored altogether. If there's a `pack` folder
 *	 | we run a `dirent` callback through every file in the pack folder
 *	 | to find our packfiles. The packfiles are then sorted according
 *	 | to a sorting callback.
 * 	 |
 *	 |-# packfile_load__cb
 *	 | | This callback is called from `dirent` with every single file
 *	 | | inside the pack folder. We find the packs by actually locating
 *	 | | their index (ends in ".idx"). From that index, we verify that
 *	 | | the corresponding packfile exists and is valid, and if so, we
 *	| | add it to the pack list.
 *	 | |
 *	 | |-# packfile_check
 *	 |		Make sure that there's a packfile to back this index, and store
 *	 |		some very basic information regarding the packfile itself,
 *	 |		such as the full path, the size, and the modification time.
 *	 |		We don't actually open the packfile to check for internal consistency.
 *	|
 *	|-# packfile_sort__cb
 *		Sort all the preloaded packs according to some specific criteria:
 *		we prioritize the "newer" packs because it's more likely they
 *		contain the objects we are looking for, and we prioritize local
 *		packs over remote ones.
 *
 *
 *
 *	Chapter 2: To be, or not to be...
 *	A standard packed `exist` query for an OID
 *	--------------------------------------------------
 *
 * # pack_backend__exists
 * | Check if the given SHA1 oid exists in any of the packs
 * | that have been loaded for our ODB.
 * |
 * |-# pack_entry_find
 *	| Iterate through all the packs that have been preloaded
 *	| (starting by the pack where the latest object was found)
 *	| to try to find the OID in one of them.
 *	|
 *	|-# pack_entry_find1
 *		| Check the index of an individual pack to see if the SHA1
 *		| OID can be found. If we can find the offset to that SHA1
 *		| inside of the index, that means the object is contained
 *		| inside of the packfile and we can stop searching.
 *		| Before returning, we verify that the packfile behing the
 *		| index we are searching still exists on disk.
 *		|
 *		|-# pack_entry_find_offset
 *		| | Mmap the actual index file to disk if it hasn't been opened
 *		| | yet, and run a binary search through it to find the OID.
 *		| | See <http://book.git-scm.com/7_the_packfile.html> for specifics
 *		| | on the Packfile Index format and how do we find entries in it.
 *		| |
 *		| |-# pack_index_open
 *		|	| Guess the name of the index based on the full path to the
 *		|	| packfile, open it and verify its contents. Only if the index
 *		|	| has not been opened already.
 *		|	|
 *		|	|-# pack_index_check
 *		|		Mmap the index file and do a quick run through the header
 *		|		to guess the index version (right now we support v1 and v2),
 *		|		and to verify that the size of the index makes sense.
 *		|
 *		|-# packfile_open
 *			See `packfile_open` in Chapter 3
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

static void pack_window_free_all(struct pack_backend *backend, struct git_pack_file *p);
static int pack_window_contains(git_mwindow *win, off_t offset);

static int packfile_sort__cb(const void *a_, const void *b_);

static int packfile_load__cb(void *_data, char *path);
static int packfile_refresh_all(struct pack_backend *backend);

static int pack_entry_find(struct git_pack_entry *e,
		struct pack_backend *backend, const git_oid *oid);

/* Can find the offset of an object given
 * a prefix of an identifier.
 * Throws GIT_EAMBIGUOUSOIDPREFIX if short oid
 * is ambiguous.
 * This method assumes that len is between
 * GIT_OID_MINPREFIXLEN and GIT_OID_HEXSZ.
 */
static int pack_entry_find_prefix(struct git_pack_entry *e,
					struct pack_backend *backend,
					const git_oid *short_oid,
					unsigned int len);



/***********************************************************
 *
 * PACK WINDOW MANAGEMENT
 *
 ***********************************************************/

GIT_INLINE(void) pack_window_free_all(struct pack_backend *GIT_UNUSED(backend), struct git_pack_file *p)
{
	GIT_UNUSED_ARG(backend);
	git_mwindow_free_all(&p->mwf);
}

GIT_INLINE(int) pack_window_contains(git_mwindow *win, off_t offset)
{
	/* We must promise at least 20 bytes (one hash) after the
	 * offset is available from this window, otherwise the offset
	 * is not actually in this window and a different window (which
	 * has that one hash excess) must be used. This is to support
	 * the object header and delta base parsing routines below.
	 */
	return git_mwindow_contains(win, offset + 20);
}

static int packfile_sort__cb(const void *a_, const void *b_)
{
	const struct git_pack_file *a = a_;
	const struct git_pack_file *b = b_;
	int st;

	/*
	 * Local packs tend to contain objects specific to our
	 * variant of the project than remote ones. In addition,
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



static int packfile_load__cb(void *_data, char *path)
{
	struct pack_backend *backend = (struct pack_backend *)_data;
	struct git_pack_file *pack;
	int error;
	size_t i;

	if (git__suffixcmp(path, ".idx") != 0)
		return GIT_SUCCESS; /* not an index */

	for (i = 0; i < backend->packs.length; ++i) {
		struct git_pack_file *p = git_vector_get(&backend->packs, i);
		if (memcmp(p->pack_name, path, strlen(path) - strlen(".idx")) == 0)
			return GIT_SUCCESS;
	}

	error = git_packfile_check(&pack, path);
	if (error == GIT_ENOTFOUND) {
		/* ignore missing .pack file as git does */
		return GIT_SUCCESS;
	} else if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to load packfile");

	if (git_vector_insert(&backend->packs, pack) < GIT_SUCCESS) {
		git__free(pack);
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

static int pack_entry_find(struct git_pack_entry *e, struct pack_backend *backend, const git_oid *oid)
{
	int error;
	size_t i;

	if ((error = packfile_refresh_all(backend)) < GIT_SUCCESS)
		return git__rethrow(error, "Failed to find pack entry");

	if (backend->last_found &&
		git_pack_entry_find(e, backend->last_found, oid, GIT_OID_HEXSZ) == GIT_SUCCESS)
		return GIT_SUCCESS;

	for (i = 0; i < backend->packs.length; ++i) {
		struct git_pack_file *p;

		p = git_vector_get(&backend->packs, i);
		if (p == backend->last_found)
			continue;

		if (git_pack_entry_find(e, p, oid, GIT_OID_HEXSZ) == GIT_SUCCESS) {
			backend->last_found = p;
			return GIT_SUCCESS;
		}
	}

	return git__throw(GIT_ENOTFOUND, "Failed to find pack entry");
}

static int pack_entry_find_prefix(
	struct git_pack_entry *e,
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
		error = git_pack_entry_find(e, backend->last_found, short_oid, len);
		if (error == GIT_EAMBIGUOUSOIDPREFIX) {
			return git__rethrow(error, "Failed to find pack entry. Ambiguous sha1 prefix");
		} else if (error == GIT_SUCCESS) {
			found = 1;
		}
	}

	for (i = 0; i < backend->packs.length; ++i) {
		struct git_pack_file *p;

		p = git_vector_get(&backend->packs, i);
		if (p == backend->last_found)
			continue;

		error = git_pack_entry_find(e, p, short_oid, len);
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

static int pack_backend__read(void **buffer_p, size_t *len_p, git_otype *type_p, git_odb_backend *backend, const git_oid *oid)
{
	struct git_pack_entry e;
	git_rawobj raw;
	int error;

	if ((error = pack_entry_find(&e, (struct pack_backend *)backend, oid)) < GIT_SUCCESS)
		return git__rethrow(error, "Failed to read pack backend");

	if ((error = git_packfile_unpack(&raw, e.p, &e.offset)) < GIT_SUCCESS)
		return git__rethrow(error, "Failed to read pack backend");

	*buffer_p = raw.data;
	*len_p = raw.len;
	*type_p = raw.type;

	return GIT_SUCCESS;
}

static int pack_backend__read_prefix(
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
		struct git_pack_entry e;
		git_rawobj raw;
		int error;

		if ((error = pack_entry_find_prefix(&e, (struct pack_backend *)backend, short_oid, len)) < GIT_SUCCESS)
			return git__rethrow(error, "Failed to read pack backend");

		if ((error = git_packfile_unpack(&raw, e.p, &e.offset)) < GIT_SUCCESS)
			return git__rethrow(error, "Failed to read pack backend");

		*buffer_p = raw.data;
		*len_p = raw.len;
		*type_p = raw.type;
		git_oid_cpy(out_oid, &e.sha1);
	}

	return GIT_SUCCESS;
}

static int pack_backend__exists(git_odb_backend *backend, const git_oid *oid)
{
	struct git_pack_entry e;
	return pack_entry_find(&e, (struct pack_backend *)backend, oid) == GIT_SUCCESS;
}

static void pack_backend__free(git_odb_backend *_backend)
{
	struct pack_backend *backend;
	size_t i;

	assert(_backend);

	backend = (struct pack_backend *)_backend;

	for (i = 0; i < backend->packs.length; ++i) {
		struct git_pack_file *p = git_vector_get(&backend->packs, i);
		packfile_free(p);
	}

	git_vector_free(&backend->packs);
	git__free(backend->pack_folder);
	git__free(backend);
}

int git_odb_backend_pack(git_odb_backend **backend_out, const char *objects_dir)
{
	struct pack_backend *backend;
	char path[GIT_PATH_MAX];

	backend = git__calloc(1, sizeof(struct pack_backend));
	if (backend == NULL)
		return GIT_ENOMEM;

	if (git_vector_init(&backend->packs, 8, packfile_sort__cb) < GIT_SUCCESS) {
		git__free(backend);
		return GIT_ENOMEM;
	}

	git_path_join(path, objects_dir, "pack");
	if (git_futils_isdir(path) == GIT_SUCCESS) {
		backend->pack_folder = git__strdup(path);
		backend->pack_folder_mtime = 0;

		if (backend->pack_folder == NULL) {
			git__free(backend);
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
