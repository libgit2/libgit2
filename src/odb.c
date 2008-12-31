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
#include "git/odb.h"
#include "git/zlib.h"
#include "fileops.h"
#include "hash.h"
#include <stdio.h>

struct git_odb {
	/** Path to the "objects" directory. */
	char *objects_dir;

	/** Alternate databases to search. */
	git_odb **alternates;
};

typedef struct {  /* object header data */
	git_otype type;  /* object type */
	size_t    size;  /* object size */
} obj_hdr;

static struct {
	const char *str;   /* type name string */
	int        loose;  /* valid loose object type flag */
} obj_type_table [] = {
	{ "",          0 },  /* 0 = GIT_OBJ__EXT1     */
	{ "commit",    1 },  /* 1 = GIT_OBJ_COMMIT    */
	{ "tree",      1 },  /* 2 = GIT_OBJ_TREE      */
	{ "blob",      1 },  /* 3 = GIT_OBJ_BLOB      */
	{ "tag",       1 },  /* 4 = GIT_OBJ_TAG       */
	{ "",          0 },  /* 5 = GIT_OBJ__EXT2     */
	{ "OFS_DELTA", 0 },  /* 6 = GIT_OBJ_OFS_DELTA */
	{ "REF_DELTA", 0 }   /* 7 = GIT_OBJ_REF_DELTA */
};

const char *git_obj_type_to_string(git_otype type)
{
	if (type < 0 || type >= ARRAY_SIZE(obj_type_table))
		return "";
	return obj_type_table[type].str;
}

git_otype git_obj_string_to_type(const char *str)
{
	int i;

	if (!str || !*str)
		return GIT_OBJ_BAD;

	for (i = 0; i < ARRAY_SIZE(obj_type_table); i++)
		if (!strcmp(str, obj_type_table[i].str))
			return (git_otype) i;

	return GIT_OBJ_BAD;
}

int git_obj__loose_object_type(git_otype type)
{
	if (type < 0 || type >= ARRAY_SIZE(obj_type_table))
		return 0;
	return obj_type_table[type].loose;
}

static int format_object_header(char *hdr, size_t n, git_obj *obj)
{
	const char *type_str = git_obj_type_to_string(obj->type);
	int len = snprintf(hdr, n, "%s %" PRIuPTR, type_str, obj->len);

	assert(len > 0);  /* otherwise snprintf() is broken */
	assert(len < n);  /* otherwise the caller is broken! */

	if (len < 0 || len >= n)
		return GIT_ERROR;
	return len+1;
}

int git_obj_hash(git_oid *id, git_obj *obj)
{
	git_buf_vec vec[2];
	char hdr[64];
	int  hdrlen;

	assert(id && obj);

	if (!git_obj__loose_object_type(obj->type))
		return GIT_ERROR;

	if (!obj->data && obj->len != 0)
		return GIT_ERROR;

	if ((hdrlen = format_object_header(hdr, sizeof(hdr), obj)) < 0)
		return GIT_ERROR;

	vec[0].data = hdr;
	vec[0].len  = hdrlen;
	vec[1].data = obj->data;
	vec[1].len  = obj->len;

	git_hash_vec(id, vec, 2);

	return GIT_SUCCESS;
}

static int object_file_name(char *name, size_t n, char *dir, const git_oid *id)
{
	size_t len = strlen(dir);

	/* check length: 43 = 40 hex sha1 chars + 2 * '/' + '\0' */
	if (len+43 > n)
		return len+43;

	/* the object dir: eg $GIT_DIR/objects */
	strcpy(name, dir);
	if (name[len-1] != '/')
		name[len++] = '/';

	/* loose object filename: aa/aaa... (41 bytes) */
	git_oid_pathfmt(&name[len], id);
	name[len+41] = '\0';

	return 0;
}

static int is_zlib_compressed_data(unsigned char *data)
{
	unsigned int w;

	w = ((unsigned int)(data[0]) << 8) + data[1];
	return data[0] == 0x78 && !(w %31);
}

static size_t get_binary_object_header(obj_hdr *hdr, gitfo_buf *obj)
{
	unsigned char c;
	unsigned char *data = obj->data;
	size_t shift, size, used = 0;

	if (obj->len == 0)
		return 0;

	c = data[used++];
	hdr->type = (c >> 4) & 7;

	size = c & 15;
	shift = 4;
	while (c & 0x80) {
		if (obj->len <= used)
			return 0;
		if (sizeof(size_t) * 8 <= shift)
			return 0;
		c = data[used++];
		size += (c & 0x7f) << shift;
		shift += 7;
	}
	hdr->size = size;

	return used;
}

static size_t get_object_header(obj_hdr *hdr, unsigned char *data)
{
	char c, typename[10];
	size_t size, used = 0;

	/*
	 * type name string followed by space.
	 */
	while ((c = data[used]) != ' ') {
		typename[used++] = c;
		if (used >= sizeof(typename))
			return 0;
	}
	typename[used] = 0;
	if (used == 0)
		return 0;
	hdr->type = git_obj_string_to_type(typename);
	used++;  /* consume the space */

	/*
	 * length follows immediately in decimal (without
	 * leading zeros).
	 */
	size = data[used++] - '0';
	if (size > 9)
		return 0;
	if (size) {
		while ((c = data[used]) != '\0') {
			size_t d = c - '0';
			if (d > 9)
				break;
			used++;
			size = size * 10 + d;
		}
	}
	hdr->size = size;

	/*
	 * the length must be followed by a zero byte
	 */
	if (data[used++] != '\0')
		return 0;

	return used;
}

static void init_stream(z_stream *s, void *out, size_t len)
{
	memset(s, 0, sizeof(*s));
	s->next_out  = out;
	s->avail_out = len;
}

static void set_stream_input(z_stream *s, void *in, size_t len)
{
	s->next_in  = in;
	s->avail_in = len;
}

static void set_stream_output(z_stream *s, void *out, size_t len)
{
	s->next_out  = out;
	s->avail_out = len;
}

static int start_inflate(z_stream *s, gitfo_buf *obj, void *out, size_t len)
{
	init_stream(s, out, len);
	set_stream_input(s, obj->data, obj->len);
	inflateInit(s);
	return inflate(s, 0);
}

static int finish_inflate(z_stream *s)
{
	int status = Z_OK;

	while (status == Z_OK)
		status = inflate(s, Z_FINISH);

	inflateEnd(s);

	if ((status != Z_STREAM_END) || (s->avail_in != 0))
		return GIT_ERROR;

	return GIT_SUCCESS;
}

static void *inflate_tail(z_stream *s, void *hb, size_t used, obj_hdr *hdr)
{
	unsigned char *buf, *head = hb;
	size_t tail;

	/*
	 * allocate a buffer to hold the inflated data and copy the
	 * initial sequence of inflated data from the tail of the
	 * head buffer, if any.
	 */
	if ((buf = git__malloc(hdr->size + 1)) == NULL)
		return NULL;
	tail = s->total_out - used;
	if (used > 0 && tail > 0) {
		if (tail > hdr->size)
			tail = hdr->size;
		memcpy(buf, head + used, tail);
	}
	used = tail;

	/*
	 * inflate the remainder of the object data, if any
	 */
	if (hdr->size >= used) {
		set_stream_output(s, buf + used, hdr->size - used);
		if (finish_inflate(s)) {
			free(buf);
			return NULL;
		}
	}

	return buf;
}

static int inflate_buffer(void *in, size_t inlen, void *out, size_t outlen)
{
	z_stream zs;
	int status = Z_OK;

	init_stream(&zs, out, outlen);
	set_stream_input(&zs, in, inlen);

	inflateInit(&zs);

	while (status == Z_OK)
		status = inflate(&zs, Z_FINISH);

	inflateEnd(&zs);

	if ((status != Z_STREAM_END) || (zs.total_out != outlen))
		return GIT_ERROR;

	if (zs.avail_in != 0)
		return GIT_ERROR;

	return GIT_SUCCESS;
}

/*
 * At one point, there was a loose object format that was intended to
 * mimic the format used in pack-files. This was to allow easy copying
 * of loose object data into packs. This format is no longer used, but
 * we must still read it.
 */
static int inflate_packlike_loose_disk_obj(git_obj *out, gitfo_buf *obj)
{
	unsigned char *in, *buf;
	obj_hdr hdr;
	size_t len, used;

	/*
	 * read the object header, which is an (uncompressed)
	 * binary encoding of the object type and size.
	 */
	if ((used = get_binary_object_header(&hdr, obj)) == 0)
		return GIT_ERROR;

	if (!git_obj__loose_object_type(hdr.type))
		return GIT_ERROR;

	/*
	 * allocate a buffer and inflate the data into it
	 */
	buf = git__malloc(hdr.size + 1);
	if (!buf)
		return GIT_ERROR;

	in  = ((unsigned char *)obj->data) + used;
	len = obj->len - used;
	if (inflate_buffer(in, len, buf, hdr.size)) {
		free(buf);
		return GIT_ERROR;
	}
	buf[hdr.size] = '\0';

	out->data = buf;
	out->len  = hdr.size;
	out->type = hdr.type;

	return GIT_SUCCESS;
}

static int inflate_disk_obj(git_obj *out, gitfo_buf *obj)
{
	unsigned char head[64], *buf;
	z_stream zs;
	int z_status;
	obj_hdr hdr;
	size_t used;

	/*
	 * check for a pack-like loose object
	 */
	if (!is_zlib_compressed_data(obj->data))
		return inflate_packlike_loose_disk_obj(out, obj);

	/*
	 * inflate the initial part of the io buffer in order
	 * to parse the object header (type and size).
	 */
	if ((z_status = start_inflate(&zs, obj, head, sizeof(head))) < Z_OK)
		return GIT_ERROR;

	if ((used = get_object_header(&hdr, head)) == 0)
		return GIT_ERROR;

	if (!git_obj__loose_object_type(hdr.type))
		return GIT_ERROR;

	/*
	 * allocate a buffer and inflate the object data into it
	 * (including the initial sequence in the head buffer).
	 */
	if ((buf = inflate_tail(&zs, head, used, &hdr)) == NULL)
		return GIT_ERROR;
	buf[hdr.size] = '\0';

	out->data = buf;
	out->len  = hdr.size;
	out->type = hdr.type;

	return GIT_SUCCESS;
}

static int open_alternates(git_odb *db)
{
	unsigned n = 0;

	db->alternates = git__malloc(sizeof(*db->alternates) * (n + 1));
	if (!db->alternates)
		return GIT_ERROR;

	db->alternates[n] = NULL;
	return GIT_SUCCESS;
}

int git_odb_open(git_odb **out, const char *objects_dir)
{
	git_odb *db = git__malloc(sizeof(*db));
	if (!db)
		return GIT_ERROR;

	db->objects_dir = git__strdup(objects_dir);
	if (!db->objects_dir) {
		free(db);
		return GIT_ERROR;
	}

	db->alternates = NULL;

	*out = db;
	return GIT_SUCCESS;
}

void git_odb_close(git_odb *db)
{
	if (!db)
		return;

	if (db->alternates) {
		git_odb **alt;
		for (alt = db->alternates; *alt; alt++)
			git_odb_close(*alt);
		free(db->alternates);
	}

	free(db->objects_dir);
	free(db);
}

int git_odb_read(
	git_obj *out,
	git_odb *db,
	const git_oid *id)
{
attempt:
	if (!git_odb__read_packed(out, db, id))
		return GIT_SUCCESS;
	if (!git_odb__read_loose(out, db, id))
		return GIT_SUCCESS;
	if (!db->alternates && !open_alternates(db))
		goto attempt;

	out->data = NULL;
	return GIT_ENOTFOUND;
}

int git_odb__read_loose(git_obj *out, git_odb *db, const git_oid *id)
{
	char file[GIT_PATH_MAX];
	gitfo_buf obj = GITFO_BUF_INIT;

	assert(out && db && id);

	out->data = NULL;
	out->len  = 0;
	out->type = GIT_OBJ_BAD;

	if (object_file_name(file, sizeof(file), db->objects_dir, id))
		return GIT_ENOTFOUND;  /* TODO: error handling */

	if (gitfo_read_file(&obj, file))
		return GIT_ENOTFOUND;  /* TODO: error handling */

	if (inflate_disk_obj(out, &obj)) {
		gitfo_free_buf(&obj);
		return GIT_ENOTFOUND;  /* TODO: error handling */
	}

	gitfo_free_buf(&obj);

	return GIT_SUCCESS;
}

int git_odb__read_packed(git_obj *out, git_odb *db, const git_oid *id)
{
	return GIT_ENOTFOUND;
}

