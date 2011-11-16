/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "odb.h"
#include "pack.h"
#include "delta-apply.h"
#include "sha1_lookup.h"
#include "mwindow.h"
#include "fileops.h"

#include "git2/oid.h"
#include "git2/zlib.h"

static int packfile_open(struct git_pack_file *p);
static off_t nth_packed_object_offset(const struct git_pack_file *p, uint32_t n);
int packfile_unpack_compressed(
		git_rawobj *obj,
		struct git_pack_file *p,
		git_mwindow **w_curs,
		off_t *curpos,
		size_t size,
		git_otype type);

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
		struct git_pack_file *p,
		const git_oid *short_oid,
		unsigned int len);

/***********************************************************
 *
 * PACK INDEX METHODS
 *
 ***********************************************************/

static void pack_index_free(struct git_pack_file *p)
{
	if (p->index_map.data) {
		git_futils_mmap_free(&p->index_map);
		p->index_map.data = NULL;
	}
}

static int pack_index_check(const char *path, struct git_pack_file *p)
{
	struct git_pack_idx_header *hdr;
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
		index += 2; /* skip index header */

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
		 * - 256 index entries 4 bytes each
		 * - 24-byte entries * nr (20-byte sha1 + 4-byte offset)
		 * - 20-byte SHA1 of the packfile
		 * - 20-byte SHA1 file checksum
		 */
		if (idx_size != 4*256 + nr * 24 + 20 + 20) {
			git_futils_mmap_free(&p->index_map);
			return git__throw(GIT_EOBJCORRUPTED, "Failed to check index. Object is corrupted");
		}
	} else if (version == 2) {
		/*
		 * Minimum size:
		 * - 8 bytes of header
		 * - 256 index entries 4 bytes each
		 * - 20-byte sha1 entry * nr
		 * - 4-byte crc entry * nr
		 * - 4-byte offset entry * nr
		 * - 20-byte SHA1 of the packfile
		 * - 20-byte SHA1 file checksum
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

static int pack_index_open(struct git_pack_file *p)
{
	char *idx_name;
	int error;

	if (p->index_map.data)
		return GIT_SUCCESS;

	idx_name = git__strdup(p->pack_name);
	strcpy(idx_name + strlen(idx_name) - strlen(".pack"), ".idx");

	error = pack_index_check(idx_name, p);
	git__free(idx_name);

	return error == GIT_SUCCESS ? GIT_SUCCESS : git__rethrow(error, "Failed to open index");
}

static unsigned char *pack_window_open(
		struct git_pack_file *p,
		git_mwindow **w_cursor,
		off_t offset,
		unsigned int *left)
{
	if (p->mwf.fd == -1 && packfile_open(p) < GIT_SUCCESS)
		return NULL;

	/* Since packfiles end in a hash of their content and it's
	 * pointless to ask for an offset into the middle of that
	 * hash, and the pack_window_contains function above wouldn't match
	 * don't allow an offset too close to the end of the file.
	 */
	if (offset > (p->mwf.size - 20))
		return NULL;

	return git_mwindow_open(&p->mwf, w_cursor, offset, 20, left);
 }

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

int git_packfile_unpack_header(
		size_t *size_p,
		git_otype *type_p,
		git_mwindow_file *mwf,
		git_mwindow **w_curs,
		off_t *curpos)
{
	unsigned char *base;
	unsigned int left;
	unsigned long used;

	/* pack_window_open() assures us we have [base, base + 20) available
	 * as a range that we can look at at. (Its actually the hash
	 * size that is assured.) With our object header encoding
	 * the maximum deflated object size is 2^137, which is just
	 * insane, so we know won't exceed what we have been given.
	 */
//	base = pack_window_open(p, w_curs, *curpos, &left);
	base = git_mwindow_open(mwf, w_curs, *curpos, 20, &left);
	if (base == NULL)
		return GIT_ENOMEM;

	used = packfile_unpack_header1(size_p, type_p, base, left);

	if (used == 0)
		return git__throw(GIT_EOBJCORRUPTED, "Header length is zero");

	*curpos += used;
	return GIT_SUCCESS;
}

static int packfile_unpack_delta(
		git_rawobj *obj,
		struct git_pack_file *p,
		git_mwindow **w_curs,
		off_t *curpos,
		size_t delta_size,
		git_otype delta_type,
		off_t obj_offset)
{
	off_t base_offset;
	git_rawobj base, delta;
	int error;

	base_offset = get_delta_base(p, w_curs, curpos, delta_type, obj_offset);
	if (base_offset == 0)
		return git__throw(GIT_EOBJCORRUPTED, "Delta offset is zero");
	if (base_offset < 0)
		return git__rethrow(base_offset, "Failed to get delta base");

	git_mwindow_close(w_curs);
	error = git_packfile_unpack(&base, p, &base_offset);

	/*
	 * TODO: git.git tries to load the base from other packfiles
	 * or loose objects.
	 *
	 * We'll need to do this in order to support thin packs.
	 */
	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Corrupted delta");

	error = packfile_unpack_compressed(&delta, p, w_curs, curpos, delta_size, delta_type);
	if (error < GIT_SUCCESS) {
		git__free(base.data);
		return git__rethrow(error, "Corrupted delta");
	}

	obj->type = base.type;
	error = git__delta_apply(obj,
			base.data, base.len,
			delta.data, delta.len);

	git__free(base.data);
	git__free(delta.data);

	/* TODO: we might want to cache this shit. eventually */
	//add_delta_base_cache(p, base_offset, base, base_size, *type);
	return error; /* error set by git__delta_apply */
}

int git_packfile_unpack(
		git_rawobj *obj,
		struct git_pack_file *p,
		off_t *obj_offset)
{
	git_mwindow *w_curs = NULL;
	off_t curpos = *obj_offset;
	int error;

	size_t size = 0;
	git_otype type;

	/*
	 * TODO: optionally check the CRC on the packfile
	 */

	obj->data = NULL;
	obj->len = 0;
	obj->type = GIT_OBJ_BAD;

	error = git_packfile_unpack_header(&size, &type, &p->mwf, &w_curs, &curpos);
	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to unpack packfile");

	switch (type) {
	case GIT_OBJ_OFS_DELTA:
	case GIT_OBJ_REF_DELTA:
		error = packfile_unpack_delta(
				obj, p, &w_curs, &curpos,
				size, type, *obj_offset);
		break;

	case GIT_OBJ_COMMIT:
	case GIT_OBJ_TREE:
	case GIT_OBJ_BLOB:
	case GIT_OBJ_TAG:
		error = packfile_unpack_compressed(
				obj, p, &w_curs, &curpos,
				size, type);
		break;

	default:
		error = GIT_EOBJCORRUPTED;
		break;
	}

	git_mwindow_close(&w_curs);

	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to unpack object");

	*obj_offset = curpos;
	return GIT_SUCCESS;
}

int packfile_unpack_compressed(
		git_rawobj *obj,
		struct git_pack_file *p,
		git_mwindow **w_curs,
		off_t *curpos,
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
	stream.avail_out = (uInt)size + 1;

	st = inflateInit(&stream);
	if (st != Z_OK) {
		git__free(buffer);
		return git__throw(GIT_EZLIB, "Error in zlib");
	}

	do {
		in = pack_window_open(p, w_curs, *curpos, &stream.avail_in);
		stream.next_in = in;
		st = inflate(&stream, Z_FINISH);

		if (!stream.avail_out)
			break; /* the payload is larger than it should be */

		*curpos += stream.next_in - in;
	} while (st == Z_OK || st == Z_BUF_ERROR);

	inflateEnd(&stream);

	if ((st != Z_STREAM_END) || stream.total_out != size) {
		git__free(buffer);
		return git__throw(GIT_EZLIB, "Error in zlib");
	}

	obj->type = type;
	obj->len = size;
	obj->data = buffer;
	return GIT_SUCCESS;
}

/*
 * curpos is where the data starts, delta_obj_offset is the where the
 * header starts
 */
off_t get_delta_base(
		struct git_pack_file *p,
		git_mwindow **w_curs,
		off_t *curpos,
		git_otype type,
		off_t delta_obj_offset)
{
	unsigned char *base_info = pack_window_open(p, w_curs, *curpos, NULL);
	off_t base_offset;
	git_oid unused;

	/* pack_window_open() assured us we have [base_info, base_info + 20)
	 * as a range that we can look at without walking off the
	 * end of the mapped window. Its actually the hash size
	 * that is assured. An OFS_DELTA longer than the hash size
	 * is stupid, as then a REF_DELTA would be smaller to store.
	 */
	if (type == GIT_OBJ_OFS_DELTA) {
		unsigned used = 0;
		unsigned char c = base_info[used++];
		base_offset = c & 127;
		while (c & 128) {
			base_offset += 1;
			if (!base_offset || MSB(base_offset, 7))
				return 0; /* overflow */
			c = base_info[used++];
			base_offset = (base_offset << 7) + (c & 127);
		}
		base_offset = delta_obj_offset - base_offset;
		if (base_offset <= 0 || base_offset >= delta_obj_offset)
			return 0; /* out of bound */
		*curpos += used;
	} else if (type == GIT_OBJ_REF_DELTA) {
		/* If we have the cooperative cache, search in it first */
		if (p->has_cache) {
			int pos;
			struct git_pack_entry key;

			git_oid_fromraw(&key.sha1, base_info);
			pos = git_vector_bsearch(&p->cache, &key);
			if (pos >= 0) {
				*curpos += 20;
				return ((struct git_pack_entry *)git_vector_get(&p->cache, pos))->offset;
			}
		}
		/* The base entry _must_ be in the same pack */
		if (pack_entry_find_offset(&base_offset, &unused, p, (git_oid *)base_info, GIT_OID_HEXSZ) < GIT_SUCCESS)
			return git__rethrow(GIT_EPACKCORRUPTED, "Base entry delta is not in the same pack");
		*curpos += 20;
	} else
		return 0;

	return base_offset;
}

/***********************************************************
 *
 * PACKFILE METHODS
 *
 ***********************************************************/

static struct git_pack_file *packfile_alloc(int extra)
{
	struct git_pack_file *p = git__malloc(sizeof(*p) + extra);
	memset(p, 0, sizeof(*p));
	p->mwf.fd = -1;
	return p;
}


void packfile_free(struct git_pack_file *p)
{
	assert(p);

	/* clear_delta_base_cache(); */
	git_mwindow_free_all(&p->mwf);

	if (p->mwf.fd != -1)
		p_close(p->mwf.fd);

	pack_index_free(p);

	git__free(p->bad_object_sha1);
	git__free(p);
}

static int packfile_open(struct git_pack_file *p)
{
	struct stat st;
	struct git_pack_header hdr;
	git_oid sha1;
	unsigned char *idx_sha1;

	if (!p->index_map.data && pack_index_open(p) < GIT_SUCCESS)
		return git__throw(GIT_ENOTFOUND, "Failed to open packfile. File not found");

	/* TODO: open with noatime */
	p->mwf.fd = p_open(p->pack_name, O_RDONLY);
	if (p->mwf.fd < 0 || p_fstat(p->mwf.fd, &st) < GIT_SUCCESS)
		return git__throw(GIT_EOSERR, "Failed to open packfile. File appears to be corrupted");

	if (git_mwindow_file_register(&p->mwf) < GIT_SUCCESS) {
		p_close(p->mwf.fd);
		return git__throw(GIT_ERROR, "Failed to register packfile windows");
	}

	/* If we created the struct before we had the pack we lack size. */
	if (!p->mwf.size) {
		if (!S_ISREG(st.st_mode))
			goto cleanup;
		p->mwf.size = (off_t)st.st_size;
	} else if (p->mwf.size != st.st_size)
		goto cleanup;

#if 0
	/* We leave these file descriptors open with sliding mmap;
	 * there is no point keeping them open across exec(), though.
	 */
	fd_flag = fcntl(p->mwf.fd, F_GETFD, 0);
	if (fd_flag < 0)
		return error("cannot determine file descriptor flags");

	fd_flag |= FD_CLOEXEC;
	if (fcntl(p->pack_fd, F_SETFD, fd_flag) == -1)
		return GIT_EOSERR;
#endif

	/* Verify we recognize this pack file format. */
	if (p_read(p->mwf.fd, &hdr, sizeof(hdr)) < GIT_SUCCESS)
		goto cleanup;

	if (hdr.hdr_signature != htonl(PACK_SIGNATURE))
		goto cleanup;

	if (!pack_version_ok(hdr.hdr_version))
		goto cleanup;

	/* Verify the pack matches its index. */
	if (p->num_objects != ntohl(hdr.hdr_entries))
		goto cleanup;

	if (p_lseek(p->mwf.fd, p->mwf.size - GIT_OID_RAWSZ, SEEK_SET) == -1)
		goto cleanup;

	if (p_read(p->mwf.fd, sha1.id, GIT_OID_RAWSZ) < GIT_SUCCESS)
		goto cleanup;

	idx_sha1 = ((unsigned char *)p->index_map.data) + p->index_map.len - 40;

	if (git_oid_cmp(&sha1, (git_oid *)idx_sha1) != 0)
		goto cleanup;

	return GIT_SUCCESS;

cleanup:
	p_close(p->mwf.fd);
	p->mwf.fd = -1;
	return git__throw(GIT_EPACKCORRUPTED, "Failed to open packfile. Pack is corrupted");
}

int git_packfile_check(struct git_pack_file **pack_out, const char *path)
{
	struct stat st;
	struct git_pack_file *p;
	size_t path_len;

	*pack_out = NULL;
	path_len = strlen(path);
	p = packfile_alloc(path_len + 2);

	/*
	 * Make sure a corresponding .pack file exists and that
	 * the index looks sane.
	 */
	path_len -= strlen(".idx");
	if (path_len < 1) {
		git__free(p);
		return git__throw(GIT_ENOTFOUND, "Failed to check packfile. Wrong path name");
	}

	memcpy(p->pack_name, path, path_len);

	strcpy(p->pack_name + path_len, ".keep");
	if (git_futils_exists(p->pack_name) == GIT_SUCCESS)
		p->pack_keep = 1;

	strcpy(p->pack_name + path_len, ".pack");
	if (p_stat(p->pack_name, &st) < GIT_SUCCESS || !S_ISREG(st.st_mode)) {
		git__free(p);
		return git__throw(GIT_ENOTFOUND, "Failed to check packfile. File not found");
	}

	/* ok, it looks sane as far as we can check without
	 * actually mapping the pack file.
	 */
	p->mwf.size = (off_t)st.st_size;
	p->pack_local = 1;
	p->mtime = (git_time_t)st.st_mtime;

	/* see if we can parse the sha1 oid in the packfile name */
	if (path_len < 40 ||
		git_oid_fromstr(&p->sha1, path + path_len - GIT_OID_HEXSZ) < GIT_SUCCESS)
		memset(&p->sha1, 0x0, GIT_OID_RAWSZ);

	*pack_out = p;
	return GIT_SUCCESS;
}

/***********************************************************
 *
 * PACKFILE ENTRY SEARCH INTERNALS
 *
 ***********************************************************/

static off_t nth_packed_object_offset(const struct git_pack_file *p, uint32_t n)
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
		struct git_pack_file *p,
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
	pos = sha1_entry_pos(index, stride, 0, lo, hi, p->num_objects, short_oid->id);

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

	if (found && len != GIT_OID_HEXSZ && pos + 1 < (int)p->num_objects) {
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

int git_pack_entry_find(
		struct git_pack_entry *e,
		struct git_pack_file *p,
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
	if (p->mwf.fd == -1 && packfile_open(p) < GIT_SUCCESS)
		return git__throw(GIT_EOSERR, "Failed to find pack entry. Packfile doesn't exist on disk");

	e->offset = offset;
	e->p = p;

	git_oid_cpy(&e->sha1, &found_oid);
	return GIT_SUCCESS;
}
