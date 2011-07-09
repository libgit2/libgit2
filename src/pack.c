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

#include "mwindow.h"
#include "odb.h"
#include "pack.h"
#include "delta-apply.h"

#include "git2/oid.h"
#include "git2/zlib.h"

unsigned char *pack_window_open(
		struct pack_file *p,
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
	 * as a range that we can look at at.  (Its actually the hash
	 * size that is assured.)  With our object header encoding
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

int packfile_unpack_delta(
		git_rawobj *obj,
		struct pack_file *p,
		git_mwindow **w_curs,
		off_t curpos,
		size_t delta_size,
		git_otype delta_type,
		off_t obj_offset)
{
	off_t base_offset;
	git_rawobj base, delta;
	int error;

	base_offset = get_delta_base(p, w_curs, &curpos, delta_type, obj_offset);
	if (base_offset == 0)
		return git__throw(GIT_EOBJCORRUPTED, "Delta offset is zero");

	git_mwindow_close(w_curs);
	error = packfile_unpack(&base, p, base_offset);

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

int packfile_unpack(
		git_rawobj *obj,
		struct pack_file *p,
		off_t obj_offset)
{
	git_mwindow *w_curs = NULL;
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

	error = git_packfile_unpack_header(&size, &type, &p->mwf, &w_curs, &curpos);
	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to unpack packfile");

	switch (type) {
	case GIT_OBJ_OFS_DELTA:
	case GIT_OBJ_REF_DELTA:
		error = packfile_unpack_delta(
				obj, p, &w_curs, curpos,
				size, type, obj_offset);
		break;

	case GIT_OBJ_COMMIT:
	case GIT_OBJ_TREE:
	case GIT_OBJ_BLOB:
	case GIT_OBJ_TAG:
		error = packfile_unpack_compressed(
				obj, p, &w_curs, curpos,
				size, type);
		break;

	default:
		error = GIT_EOBJCORRUPTED;
		break;
	}

	git_mwindow_close(&w_curs);
	return error == GIT_SUCCESS ? GIT_SUCCESS : git__rethrow(error, "Failed to unpack packfile");
}

int packfile_unpack_compressed(
		git_rawobj *obj,
		struct pack_file *p,
		git_mwindow **w_curs,
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
		in = pack_window_open(p, w_curs, curpos, &stream.avail_in);
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

off_t get_delta_base(
		struct pack_file *p,
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
