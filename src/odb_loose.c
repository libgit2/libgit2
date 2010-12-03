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
#include "git/zlib.h"
#include "git/object.h"
#include "fileops.h"
#include "hash.h"
#include "odb.h"
#include "delta-apply.h"

#include "git/odb_backend.h"

typedef struct {  /* object header data */
	git_otype type;  /* object type */
	size_t    size;  /* object size */
} obj_hdr;

typedef struct loose_backend {
	git_odb_backend parent;

	int object_zlib_level; /** loose object zlib compression level. */
	int fsync_object_files; /** loose object file fsync flag. */
	char *objects_dir;
} loose_backend;


/***********************************************************
 *
 * MISCELANEOUS HELPER FUNCTIONS
 *
 ***********************************************************/

static int make_temp_file(git_file *fd, char *tmp, size_t n, char *file)
{
	char *template = "/tmp_obj_XXXXXX";
	size_t tmplen = strlen(template);
	int dirlen;

	if ((dirlen = git__dirname(tmp, n, file)) < 0)
		return GIT_ERROR;

	if ((dirlen + tmplen) >= n)
		return GIT_ERROR;

	strcpy(tmp + dirlen, (dirlen) ? template : template + 1);

	*fd = gitfo_mkstemp(tmp);
	if (*fd < 0 && dirlen) {
		/* create directory if it doesn't exist */
		tmp[dirlen] = '\0';
		if ((gitfo_exists(tmp) < 0) && gitfo_mkdir(tmp, 0755))
			return GIT_ERROR;
		/* try again */
		strcpy(tmp + dirlen, template);
		*fd = gitfo_mkstemp(tmp);
	}
	if (*fd < 0)
		return GIT_ERROR;

	return GIT_SUCCESS;
}



static size_t object_file_name(char *name, size_t n, char *dir, const git_oid *id)
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
	hdr->type = git_object_string2type(typename);
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



/***********************************************************
 *
 * ZLIB RELATED FUNCTIONS
 *
 ***********************************************************/

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
	int status;

	init_stream(s, out, len);
	set_stream_input(s, obj->data, obj->len);

	if ((status = inflateInit(s)) < Z_OK)
		return status;

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

static int deflate_buf(z_stream *s, void *in, size_t len, int flush)
{
	int status = Z_OK;

	set_stream_input(s, in, len);
	while (status == Z_OK) {
		status = deflate(s, flush);
		if (s->avail_in == 0)
			break;
	}
	return status;
}

static int deflate_obj(gitfo_buf *buf, char *hdr, int hdrlen, git_rawobj *obj, int level)
{
	z_stream zs;
	int status;
	size_t size;

	assert(buf && !buf->data && hdr && obj);
	assert(level == Z_DEFAULT_COMPRESSION || (level >= 0 && level <= 9));

	buf->data = NULL;
	buf->len  = 0;
	init_stream(&zs, NULL, 0);

	if (deflateInit(&zs, level) < Z_OK)
		return GIT_ERROR;

	size = deflateBound(&zs, hdrlen + obj->len);

	if ((buf->data = git__malloc(size)) == NULL) {
		deflateEnd(&zs);
		return GIT_ERROR;
	}

	set_stream_output(&zs, buf->data, size);

	/* compress the header */
	status = deflate_buf(&zs, hdr, hdrlen, Z_NO_FLUSH);

	/* if header compressed OK, compress the object */
	if (status == Z_OK)
		status = deflate_buf(&zs, obj->data, obj->len, Z_FINISH);

	if (status != Z_STREAM_END) {
		deflateEnd(&zs);
		free(buf->data);
		buf->data = NULL;
		return GIT_ERROR;
	}

	buf->len = zs.total_out;
	deflateEnd(&zs);

	return GIT_SUCCESS;
}

static int is_zlib_compressed_data(unsigned char *data)
{
	unsigned int w;

	w = ((unsigned int)(data[0]) << 8) + data[1];
	return data[0] == 0x78 && !(w % 31);
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
	if ((buf = git__malloc(hdr->size + 1)) == NULL) {
		inflateEnd(s);
		return NULL;
	}
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
	if (hdr->size < used)
		inflateEnd(s);
	else {
		set_stream_output(s, buf + used, hdr->size - used);
		if (finish_inflate(s)) {
			free(buf);
			return NULL;
		}
	}

	return buf;
}

/*
 * At one point, there was a loose object format that was intended to
 * mimic the format used in pack-files. This was to allow easy copying
 * of loose object data into packs. This format is no longer used, but
 * we must still read it.
 */
static int inflate_packlike_loose_disk_obj(git_rawobj *out, gitfo_buf *obj)
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

	if (!git_object_typeisloose(hdr.type))
		return GIT_ERROR;

	/*
	 * allocate a buffer and inflate the data into it
	 */
	buf = git__malloc(hdr.size + 1);
	if (!buf)
		return GIT_ERROR;

	in  = ((unsigned char *)obj->data) + used;
	len = obj->len - used;
	if (git_odb__inflate_buffer(in, len, buf, hdr.size)) {
		free(buf);
		return GIT_ERROR;
	}
	buf[hdr.size] = '\0';

	out->data = buf;
	out->len  = hdr.size;
	out->type = hdr.type;

	return GIT_SUCCESS;
}

static int inflate_disk_obj(git_rawobj *out, gitfo_buf *obj)
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

	if (!git_object_typeisloose(hdr.type))
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






/***********************************************************
 *
 * ODB OBJECT READING & WRITING
 *
 * Backend for the public API; read headers and full objects
 * from the ODB. Write raw data to the ODB.
 *
 ***********************************************************/

static int read_loose(git_rawobj *out, const char *loc)
{
	int error;
	gitfo_buf obj = GITFO_BUF_INIT;

	assert(out && loc);

	out->data = NULL;
	out->len  = 0;
	out->type = GIT_OBJ_BAD;

	if (gitfo_read_file(&obj, loc) < 0)
		return GIT_ENOTFOUND;

	error = inflate_disk_obj(out, &obj);
	gitfo_free_buf(&obj);

	return error;
}

static int read_header_loose(git_rawobj *out, const char *loc)
{
	int error = GIT_SUCCESS, z_return = Z_ERRNO, read_bytes;
	git_file fd;
	z_stream zs;
	obj_hdr header_obj;
	unsigned char raw_buffer[16], inflated_buffer[64];

	assert(out && loc);

	out->data = NULL;

	if ((fd = gitfo_open(loc, O_RDONLY)) < 0)
		return GIT_ENOTFOUND;

	init_stream(&zs, inflated_buffer, sizeof(inflated_buffer));

	if (inflateInit(&zs) < Z_OK) {
		error = GIT_EZLIB;
		goto cleanup;
	}

	do {
		if ((read_bytes = read(fd, raw_buffer, sizeof(raw_buffer))) > 0) {
			set_stream_input(&zs, raw_buffer, read_bytes);
			z_return = inflate(&zs, 0);
		}
	} while (z_return == Z_OK);

	if ((z_return != Z_STREAM_END && z_return != Z_BUF_ERROR)
		|| get_object_header(&header_obj, inflated_buffer) == 0
		|| git_object_typeisloose(header_obj.type) == 0) {
		error = GIT_EOBJCORRUPTED;
		goto cleanup;
	}

	out->len  = header_obj.size;
	out->type = header_obj.type;

cleanup:
	finish_inflate(&zs);
	gitfo_close(fd);
	return error;
}

static int write_obj(gitfo_buf *buf, git_oid *id, loose_backend *backend)
{
	char file[GIT_PATH_MAX];
	char temp[GIT_PATH_MAX];
	git_file fd;

	if (object_file_name(file, sizeof(file), backend->objects_dir, id))
		return GIT_EOSERR;

	if (make_temp_file(&fd, temp, sizeof(temp), file) < 0)
		return GIT_EOSERR;

	if (gitfo_write(fd, buf->data, buf->len) < 0) {
		gitfo_close(fd);
		gitfo_unlink(temp);
		return GIT_EOSERR;
	}

	if (backend->fsync_object_files)
		gitfo_fsync(fd);
	gitfo_close(fd);
	gitfo_chmod(temp, 0444);

	if (gitfo_move_file(temp, file) < 0) {
		gitfo_unlink(temp);
		return GIT_EOSERR;
	}

	return GIT_SUCCESS;
}

static int locate_object(char *object_location, loose_backend *backend, const git_oid *oid)
{
	object_file_name(object_location, GIT_PATH_MAX, backend->objects_dir, oid);
	return gitfo_exists(object_location);
}









/***********************************************************
 *
 * LOOSE BACKEND PUBLIC API
 *
 * Implement the git_odb_backend API calls
 *
 ***********************************************************/

int loose_backend__read_header(git_rawobj *obj, git_odb_backend *backend, const git_oid *oid)
{
	char object_path[GIT_PATH_MAX];

	assert(obj && backend && oid);

	if (locate_object(object_path, (loose_backend *)backend, oid) < 0)
		return GIT_ENOTFOUND;

	return read_header_loose(obj, object_path);
}


int loose_backend__read(git_rawobj *obj, git_odb_backend *backend, const git_oid *oid)
{
	char object_path[GIT_PATH_MAX];

	assert(obj && backend && oid);

	if (locate_object(object_path, (loose_backend *)backend, oid) < 0)
		return GIT_ENOTFOUND;

	return read_loose(obj, object_path);
}

int loose_backend__exists(git_odb_backend *backend, const git_oid *oid)
{
	char object_path[GIT_PATH_MAX];

	assert(backend && oid);

	return locate_object(object_path, (loose_backend *)backend, oid) == GIT_SUCCESS;
}


int loose_backend__write(git_oid *id, git_odb_backend *_backend, git_rawobj *obj)
{
	char hdr[64];
	int  hdrlen;
	gitfo_buf buf = GITFO_BUF_INIT;
	int error;
	loose_backend *backend;

	assert(id && _backend && obj);

	backend = (loose_backend *)_backend;

	if ((error = git_odb__hash_obj(id, hdr, sizeof(hdr), &hdrlen, obj)) < 0)
		return error;

	if (git_odb_exists(_backend->odb, id))
		return GIT_SUCCESS;

	if ((error = deflate_obj(&buf, hdr, hdrlen, obj, backend->object_zlib_level)) < 0)
		return error;

	error = write_obj(&buf, id, backend);

	gitfo_free_buf(&buf);
	return error;
}

void loose_backend__free(git_odb_backend *_backend)
{
	loose_backend *backend;
	assert(_backend);
	backend = (loose_backend *)_backend;

	free(backend->objects_dir);
	free(backend);
}

int git_odb_backend_loose(git_odb_backend **backend_out, const char *objects_dir)
{
	loose_backend *backend;

	backend = git__calloc(1, sizeof(loose_backend));
	if (backend == NULL)
		return GIT_ENOMEM;

	backend->objects_dir = git__strdup(objects_dir);
	if (backend->objects_dir == NULL) {
		free(backend);
		return GIT_ENOMEM;
	}

	backend->object_zlib_level = Z_BEST_SPEED;
	backend->fsync_object_files = 0;

	backend->parent.read = &loose_backend__read;
	backend->parent.read_header = &loose_backend__read_header;
	backend->parent.write = &loose_backend__write;
	backend->parent.exists = &loose_backend__exists;
	backend->parent.free = &loose_backend__free;

	backend->parent.priority = 2; /* higher than packfiles */

	*backend_out = (git_odb_backend *)backend;
	return GIT_SUCCESS;
}
