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
#include "fileops.h"
#include "hash.h"
#include "odb.h"
#include "delta-apply.h"

/** First 4 bytes of a pack-*.idx file header.
 *
 * Note this header exists only in idx v2 and later.  The idx v1
 * file format does not have a magic sequence at the front, and
 * must be detected by the first four bytes *not* being this value
 * and the first 8 bytes matching the following expression:
 *
 *   uint32_t *fanout = ... the file data at offset 0 ...
 *   ntohl(fanout[0]) < ntohl(fanout[1])
 *
 * The value chosen here for PACK_TOC is such that the above
 * cannot be true for an idx v1 file.
 */
#define PACK_TOC 0xff744f63 /* -1tOc */

/** First 4 bytes of a pack-*.pack file header. */
#define PACK_SIG 0x5041434b /* PACK */

#define GIT_PACK_NAME_MAX (5 + 40 + 1)

struct pack_backend;

typedef struct {
	uint32_t      n;
	unsigned char *oid;
	off_t         offset;
	off_t         size;
} index_entry;

typedef struct { /* '.pack' file header */
	uint32_t sig; /* PACK_SIG */
	uint32_t ver; /* pack version */
	uint32_t cnt; /* object count */
} pack_hdr;

typedef struct git_pack {
	struct pack_backend *backend;
	git_lck lock;

	/** Functions to access idx_map. */
	int (*idx_search)(
		uint32_t *,
		struct git_pack *,
		const git_oid *);
	int (*idx_search_offset)(
		uint32_t *,
		struct git_pack *,
		off_t);
	int (*idx_get)(
		index_entry *,
		struct git_pack *,
		uint32_t n);

	/** The .idx file, mapped into memory. */
	git_file idx_fd;
	git_map idx_map;
	uint32_t *im_fanout;
	unsigned char *im_oid;
	uint32_t *im_crc;
	uint32_t *im_offset32;
	uint32_t *im_offset64;
	uint32_t *im_off_idx;
	uint32_t *im_off_next;

	/** Number of objects in this pack. */
	uint32_t obj_cnt;

	/** File descriptor for the .pack file. */
	git_file pack_fd;

	/** Memory map of the pack's contents */
	git_map pack_map;

	/** The size of the .pack file. */
	off_t pack_size;

	/** The mtime of the .pack file. */
	time_t pack_mtime;

	/** Number of git_packlist we appear in. */
	unsigned int refcnt;

	/** Number of active users of the idx_map data. */
	unsigned int idxcnt;
	unsigned
		invalid:1 /* the pack is unable to be read by libgit2 */
		;

	/** Name of the pack file(s), without extension ("pack-abc"). */
	char pack_name[GIT_PACK_NAME_MAX];
} git_pack;

typedef struct {
	size_t n_packs;
	unsigned int refcnt;
	git_pack *packs[GIT_FLEX_ARRAY];
} git_packlist;

typedef struct pack_backend {
	git_odb_backend parent;

	git_lck lock;
	char *objects_dir;
	git_packlist *packlist;
} pack_backend;


typedef struct pack_location {
	git_pack *ptr;
	uint32_t n;
} pack_location;

static int pack_stat(git_pack *p);
static int pack_openidx(git_pack *p);
static void pack_decidx(git_pack *p);
static int read_pack_hdr(pack_hdr *out, git_file fd);
static int check_pack_hdr(git_pack *p);
static int check_pack_sha1(git_pack *p);
static int open_pack(git_pack *p);


static int pack_openidx_map(git_pack *p);
static int pack_openidx_v1(git_pack *p);
static int pack_openidx_v2(git_pack *p);


GIT_INLINE(uint32_t) decode32(void *b)
{
	return ntohl(*((uint32_t *)b));
}

GIT_INLINE(uint64_t) decode64(void *b)
{
	uint32_t *p = b;
	return (((uint64_t)ntohl(p[0])) << 32) | ntohl(p[1]);
}



/***********************************************************
 *
 * PACKFILE FUNCTIONS
 *
 * Locate, open and access the contents of a packfile
 *
 ***********************************************************/

static int pack_stat(git_pack *p)
{
	char pb[GIT_PATH_MAX];
	struct stat sb;

	if (git__fmt(pb, sizeof(pb), "%s/pack/%s.pack",
			p->backend->objects_dir,
			p->pack_name) < 0)
		return GIT_ERROR;

	if (gitfo_stat(pb, &sb) || !S_ISREG(sb.st_mode))
		return GIT_ERROR;

	if (sb.st_size < (3 * 4 + GIT_OID_RAWSZ))
		return GIT_ERROR;

	p->pack_size = sb.st_size;
	p->pack_mtime = sb.st_mtime;

	return GIT_SUCCESS;
}

static int pack_openidx(git_pack *p)
{
	gitlck_lock(&p->lock);

	if (p->invalid) {
		gitlck_unlock(&p->lock);
		return GIT_ERROR;
	}

	if (++p->idxcnt == 1 && !p->idx_search) {
		int status, version;
		uint32_t *data;

		if (pack_stat(p) || pack_openidx_map(p)) {
			p->invalid = 1;
			p->idxcnt--;
			gitlck_unlock(&p->lock);
			return GIT_ERROR;
		}
		data = p->idx_map.data;
		status = GIT_SUCCESS;
		version = 1;

		if (decode32(&data[0]) == PACK_TOC)
			version = decode32(&data[1]);

		switch (version) {
		case 1:
			status = pack_openidx_v1(p);
			break;
		case 2:
			status = pack_openidx_v2(p);
			break;
		default:
			status = GIT_ERROR;
		}

		if (status != GIT_SUCCESS) {
			gitfo_free_map(&p->idx_map);
			p->invalid = 1;
			p->idxcnt--;
			gitlck_unlock(&p->lock);
			return status;
		}
	}

	gitlck_unlock(&p->lock);
	return GIT_SUCCESS;
}

static void pack_decidx(git_pack *p)
{
	gitlck_lock(&p->lock);
	p->idxcnt--;
	gitlck_unlock(&p->lock);
}

static int read_pack_hdr(pack_hdr *out, git_file fd)
{
	pack_hdr hdr;

	if (gitfo_read(fd, &hdr, sizeof(hdr)))
		return GIT_ERROR;

	out->sig = decode32(&hdr.sig);
	out->ver = decode32(&hdr.ver);
	out->cnt = decode32(&hdr.cnt);

	return GIT_SUCCESS;
}

static int check_pack_hdr(git_pack *p)
{
	pack_hdr hdr;

	if (read_pack_hdr(&hdr, p->pack_fd))
		return GIT_ERROR;

	if (hdr.sig != PACK_SIG
		|| (hdr.ver != 2 && hdr.ver != 3)
		|| hdr.cnt != p->obj_cnt)
		return GIT_ERROR;

	return GIT_SUCCESS;
}

static int check_pack_sha1(git_pack *p)
{
	unsigned char *data = p->idx_map.data;
	off_t pack_sha1_off = p->pack_size - GIT_OID_RAWSZ;
	size_t idx_pack_sha1_off = p->idx_map.len - 2 * GIT_OID_RAWSZ;
	git_oid pack_id, idx_pack_id;

	if (gitfo_lseek(p->pack_fd, pack_sha1_off, SEEK_SET) == -1)
		return GIT_ERROR;

	if (gitfo_read(p->pack_fd, pack_id.id, sizeof(pack_id.id)))
		return GIT_ERROR;

	git_oid_mkraw(&idx_pack_id, data + idx_pack_sha1_off);

	if (git_oid_cmp(&pack_id, &idx_pack_id))
		return GIT_ERROR;

	return GIT_SUCCESS;
}

static int open_pack(git_pack *p)
{
	char pb[GIT_PATH_MAX];
	struct stat sb;

	if (p->pack_fd != -1)
		return GIT_SUCCESS;

	if (git__fmt(pb, sizeof(pb), "%s/pack/%s.pack",
			p->backend->objects_dir,
			p->pack_name) < 0)
		return GIT_ERROR;

	if (pack_openidx(p))
		return GIT_ERROR;

	if ((p->pack_fd = gitfo_open(pb, O_RDONLY)) < 0)
		goto error_cleanup;

	if (gitfo_fstat(p->pack_fd, &sb)
		|| !S_ISREG(sb.st_mode) || p->pack_size != sb.st_size
		|| check_pack_hdr(p) || check_pack_sha1(p))
		goto error_cleanup;

	if (!git__is_sizet(p->pack_size) ||
		gitfo_map_ro(&p->pack_map, p->pack_fd, 0, (size_t)p->pack_size) < 0)
		goto error_cleanup;

	pack_decidx(p);
	return GIT_SUCCESS;

error_cleanup:
	gitfo_close(p->pack_fd);
	p->pack_fd = -1;
	pack_decidx(p);
	return GIT_ERROR;
}

static void pack_dec(git_pack *p)
{
	int need_free;

	gitlck_lock(&p->lock);
	need_free = !--p->refcnt;
	gitlck_unlock(&p->lock);

	if (need_free) {
		if (p->idx_search) {
			gitfo_free_map(&p->idx_map);
			gitfo_close(p->idx_fd);
			free(p->im_fanout);
			free(p->im_off_idx);
			free(p->im_off_next);
			if (p->pack_fd != -1) {
				gitfo_close(p->pack_fd);
				gitfo_free_map(&p->pack_map);
			}
		}

		gitlck_free(&p->lock);
		free(p);
	}
}

static void packlist_dec(pack_backend *backend, git_packlist *pl)
{
	int need_free;

	assert(backend && pl);

	gitlck_lock(&backend->lock);
	need_free = !--pl->refcnt;
	gitlck_unlock(&backend->lock);

	if (need_free) {
		size_t j;
		for (j = 0; j < pl->n_packs; j++)
			pack_dec(pl->packs[j]);
		free(pl);
	}
}

static git_pack *alloc_pack(const char *pack_name)
{
	git_pack *p = git__calloc(1, sizeof(*p));
	if (!p)
		return NULL;

	gitlck_init(&p->lock);
	strcpy(p->pack_name, pack_name);
	p->refcnt = 1;
	p->pack_fd = -1;
	return p;
}

struct scanned_pack {
	struct scanned_pack *next;
	git_pack *pack;
};

static int scan_one_pack(void *state, char *name)
{
	struct scanned_pack **ret = state, *r;
	char *s = strrchr(name, '/'), *d;

	if (git__prefixcmp(s + 1, "pack-")
	 || git__suffixcmp(s, ".pack")
	 || strlen(s + 1) != GIT_PACK_NAME_MAX + 4)
		return 0;

	d = strrchr(s + 1, '.');
	strcpy(d + 1, "idx");    /* "pack-abc.pack" -> "pack-abc.idx" */
	if (gitfo_exists(name))
		return 0;

	if ((r = git__malloc(sizeof(*r))) == NULL)
		return GIT_ERROR;

	*d = '\0';               /* "pack-abc.pack" -_> "pack-abc" */
	if ((r->pack = alloc_pack(s + 1)) == NULL) {
		free(r);
		return GIT_ERROR;
	}

	r->next = *ret;
	*ret = r;
	return 0;
}

static git_packlist *scan_packs(pack_backend *backend)
{
	char pb[GIT_PATH_MAX];
	struct scanned_pack *state = NULL, *c;
	size_t cnt;
	git_packlist *new_list;

	if (git__fmt(pb, sizeof(pb), "%s/pack", backend->objects_dir) < 0)
		return NULL;
	gitfo_dirent(pb, sizeof(pb), scan_one_pack, &state);

	/* TODO - merge old entries into the new array */
	for (cnt = 0, c = state; c; c = c->next)
		cnt++;
	new_list = git__malloc(sizeof(*new_list)
		+ (sizeof(new_list->packs[0]) * cnt));
	if (!new_list)
		goto fail;

	for (cnt = 0, c = state; c; ) {
		struct scanned_pack *n = c->next;
		c->pack->backend = backend;
		new_list->packs[cnt++] = c->pack;
		free(c);
		c = n;
	}
	new_list->n_packs = cnt;
	new_list->refcnt = 2;
	backend->packlist = new_list;
	return new_list;

fail:
	while (state) {
		struct scanned_pack *n = state->next;
		pack_dec(state->pack);
		free(state);
		state = n;
	}
	return NULL;
}

static git_packlist *packlist_get(pack_backend *backend)
{
	git_packlist *pl;

	gitlck_lock(&backend->lock);
	if ((pl = backend->packlist) != NULL)
		pl->refcnt++;
	else
		pl = scan_packs(backend);
	gitlck_unlock(&backend->lock);
	return pl;
}

static int locate_packfile(pack_location *location, pack_backend *backend, const git_oid *id)
{
	git_packlist *pl = packlist_get(backend);
	size_t j;

	if (!pl)
		return GIT_ENOTFOUND;

	for (j = 0; j < pl->n_packs; j++) {

		git_pack *pack = pl->packs[j];
		uint32_t pos;
		int res;

		if (pack_openidx(pack))
			continue;

		res = pack->idx_search(&pos, pack, id);
		pack_decidx(pack);

		if (!res) {
			packlist_dec(backend, pl);

			location->ptr = pack;
			location->n = pos;

			return GIT_SUCCESS;
		}

	}

	packlist_dec(backend, pl);
	return GIT_ENOTFOUND;
}













/***********************************************************
 *
 * PACKFILE INDEX FUNCTIONS
 *
 * Get index formation for packfile indexes v1 and v2
 *
 ***********************************************************/

static int pack_openidx_map(git_pack *p)
{
	char pb[GIT_PATH_MAX];
	off_t len;

	if (git__fmt(pb, sizeof(pb), "%s/pack/%s.idx",
			p->backend->objects_dir,
			p->pack_name) < 0)
		return GIT_ERROR;

	if ((p->idx_fd = gitfo_open(pb, O_RDONLY)) < 0)
		return GIT_ERROR;

	if ((len = gitfo_size(p->idx_fd)) < 0
		|| !git__is_sizet(len)
		|| gitfo_map_ro(&p->idx_map, p->idx_fd, 0, (size_t)len)) {
		gitfo_close(p->idx_fd);
		return GIT_ERROR;
	}

	return GIT_SUCCESS;
}

typedef struct {
	off_t offset;
	uint32_t n;
} offset_idx_info;

static int cmp_offset_idx_info(const void *lhs, const void *rhs)
{
	const offset_idx_info *a = lhs;
	const offset_idx_info *b = rhs;
	return (a->offset < b->offset) ? -1 : (a->offset > b->offset) ? 1 : 0;
}

static int make_offset_index(git_pack *p, offset_idx_info *data)
{
	off_t min_off = 3 * 4, max_off = p->pack_size - GIT_OID_RAWSZ;
	uint32_t *idx, *next;
	uint32_t j;

	qsort(data, p->obj_cnt, sizeof(*data), cmp_offset_idx_info);

	if (data[0].offset < min_off || data[p->obj_cnt].offset > max_off)
		return GIT_ERROR;

	if ((idx = git__malloc(sizeof(*idx) * (p->obj_cnt+1))) == NULL)
		return GIT_ERROR;
	if ((next = git__malloc(sizeof(*next) * p->obj_cnt)) == NULL) {
		free(idx);
		return GIT_ERROR;
	}

	for (j = 0; j < p->obj_cnt+1; j++)
		idx[j] = data[j].n;

	for (j = 0; j < p->obj_cnt; j++) {
		assert(idx[j]   < p->obj_cnt);
		assert(idx[j+1] < p->obj_cnt+1);

		next[idx[j]] = idx[j+1];
	}

	p->im_off_idx = idx;
	p->im_off_next = next;
	return GIT_SUCCESS;
}

static int idxv1_search(uint32_t *out, git_pack *p, const git_oid *id)
{
	unsigned char *data = p->im_oid;
	uint32_t lo = id->id[0] ? p->im_fanout[id->id[0] - 1] : 0;
	uint32_t hi = p->im_fanout[id->id[0]];

	do {
		uint32_t mid = (lo + hi) >> 1;
		uint32_t pos = 24 * mid;
		int cmp = memcmp(id->id, data + pos + 4, 20);
		if (cmp < 0)
			hi = mid;
		else if (!cmp) {
			*out = mid;
			return GIT_SUCCESS;
		} else
			lo = mid + 1;
	} while (lo < hi);
	return GIT_ENOTFOUND;
}

static int idxv1_search_offset(uint32_t *out, git_pack *p, off_t offset)
{
	if (offset > 0 && offset < (p->pack_size - GIT_OID_RAWSZ)) {
		uint32_t lo = 0, hi = p->obj_cnt+1;
		unsigned char *data = p->im_oid;
		uint32_t *idx = p->im_off_idx;
		do {
			uint32_t mid = (lo + hi) >> 1;
			uint32_t n = idx[mid];
			uint32_t pos = n * (GIT_OID_RAWSZ + 4);
			off_t here = decode32(data + pos);
			if (offset < here)
				hi = mid;
			else if (offset == here) {
				*out = n;
				return GIT_SUCCESS;
			} else
				lo = mid + 1;
		} while (lo < hi);
	}
	return GIT_ENOTFOUND;
}

static int idxv1_get(index_entry *e, git_pack *p, uint32_t n)
{
	unsigned char *data = p->im_oid;
	uint32_t *next = p->im_off_next;

	if (n < p->obj_cnt) {
		uint32_t pos = n * (GIT_OID_RAWSZ + 4);
		off_t next_off = p->pack_size - GIT_OID_RAWSZ;
		e->n = n;
		e->oid = data + pos + 4;
		e->offset = decode32(data + pos);
		if (next[n] < p->obj_cnt) {
			pos = next[n] * (GIT_OID_RAWSZ + 4);
			next_off = decode32(data + pos);
		}
		e->size = next_off - e->offset;
		return GIT_SUCCESS;
	}
	return GIT_ENOTFOUND;
}

static int pack_openidx_v1(git_pack *p)
{
	uint32_t *src_fanout = p->idx_map.data;
	uint32_t *im_fanout;
	offset_idx_info *info;
	size_t expsz;
	uint32_t j;


	if ((im_fanout = git__malloc(sizeof(*im_fanout) * 256)) == NULL)
		return GIT_ERROR;

	im_fanout[0] = decode32(&src_fanout[0]);
	for (j = 1; j < 256; j++) {
		im_fanout[j] = decode32(&src_fanout[j]);
		if (im_fanout[j] < im_fanout[j - 1]) {
			free(im_fanout);
			return GIT_ERROR;
		}
	}
	p->obj_cnt = im_fanout[255];

	expsz = 4 * 256 + 24 * p->obj_cnt + 2 * 20;
	if (expsz != p->idx_map.len) {
		free(im_fanout);
		return GIT_ERROR;
	}

	p->idx_search = idxv1_search;
	p->idx_search_offset = idxv1_search_offset;
	p->idx_get = idxv1_get;
	p->im_fanout = im_fanout;
	p->im_oid = (unsigned char *)(src_fanout + 256);

	if ((info = git__malloc(sizeof(*info) * (p->obj_cnt+1))) == NULL) {
		free(im_fanout);
		return GIT_ERROR;
	}

	for (j = 0; j < p->obj_cnt; j++) {
		uint32_t pos = j * (GIT_OID_RAWSZ + 4);
		info[j].offset = decode32(p->im_oid + pos);
		info[j].n = j;
	}
	info[p->obj_cnt].offset = p->pack_size - GIT_OID_RAWSZ;
	info[p->obj_cnt].n = p->obj_cnt;

	if (make_offset_index(p, info)) {
		free(im_fanout);
		free(info);
		return GIT_ERROR;
	}
	free(info);

	return GIT_SUCCESS;
}

static int idxv2_search(uint32_t *out, git_pack *p, const git_oid *id)
{
	unsigned char *data = p->im_oid;
	uint32_t lo = id->id[0] ? p->im_fanout[id->id[0] - 1] : 0;
	uint32_t hi = p->im_fanout[id->id[0]];

	do {
		uint32_t mid = (lo + hi) >> 1;
		uint32_t pos = 20 * mid;
		int cmp = memcmp(id->id, data + pos, 20);
		if (cmp < 0)
			hi = mid;
		else if (!cmp) {
			*out = mid;
			return GIT_SUCCESS;
		} else
			lo = mid + 1;
	} while (lo < hi);
	return GIT_ENOTFOUND;
}

static int idxv2_search_offset(uint32_t *out, git_pack *p, off_t offset)
{
	if (offset > 0 && offset < (p->pack_size - GIT_OID_RAWSZ)) {
		uint32_t lo = 0, hi = p->obj_cnt+1;
		uint32_t *idx = p->im_off_idx;
		do {
			uint32_t mid = (lo + hi) >> 1;
			uint32_t n = idx[mid];
			uint32_t o32 = decode32(p->im_offset32 + n);
			off_t here = o32;

			if (o32 & 0x80000000) {
				uint32_t o64_idx = (o32 & ~0x80000000);
				here = decode64(p->im_offset64 + 2*o64_idx);
			}

			if (offset < here)
				hi = mid;
			else if (offset == here) {
				*out = n;
				return GIT_SUCCESS;
			} else
				lo = mid + 1;
		} while (lo < hi);
	}
	return GIT_ENOTFOUND;
}

static int idxv2_get(index_entry *e, git_pack *p, uint32_t n)
{
	unsigned char *data = p->im_oid;
	uint32_t *next = p->im_off_next;

	if (n < p->obj_cnt) {
		uint32_t o32 = decode32(p->im_offset32 + n);
		off_t next_off = p->pack_size - GIT_OID_RAWSZ;
		e->n = n;
		e->oid = data + n * GIT_OID_RAWSZ;
		e->offset = o32;
		if (o32 & 0x80000000) {
			uint32_t o64_idx = (o32 & ~0x80000000);
			e->offset = decode64(p->im_offset64 + 2*o64_idx);
		}
		if (next[n] < p->obj_cnt) {
			o32 = decode32(p->im_offset32 + next[n]);
			next_off = o32;
			if (o32 & 0x80000000) {
				uint32_t o64_idx = (o32 & ~0x80000000);
				next_off = decode64(p->im_offset64 + 2*o64_idx);
			}
		}
		e->size = next_off - e->offset;
		return GIT_SUCCESS;
	}
	return GIT_ENOTFOUND;
}

static int pack_openidx_v2(git_pack *p)
{
	unsigned char *data = p->idx_map.data;
	uint32_t *src_fanout = (uint32_t *)(data + 8);
	uint32_t *im_fanout;
	offset_idx_info *info;
	size_t sz, o64_sz, o64_len;
	uint32_t j;

	if ((im_fanout = git__malloc(sizeof(*im_fanout) * 256)) == NULL)
		return GIT_ERROR;

	im_fanout[0] = decode32(&src_fanout[0]);
	for (j = 1; j < 256; j++) {
		im_fanout[j] = decode32(&src_fanout[j]);
		if (im_fanout[j] < im_fanout[j - 1]) {
			free(im_fanout);
			return GIT_ERROR;
		}
	}
	p->obj_cnt = im_fanout[255];

	/* minimum size of .idx file (with empty 64-bit offsets table): */
	sz = 4 + 4 + 256 * 4 + p->obj_cnt * (20 + 4 + 4) + 2 * 20;
	if (p->idx_map.len < sz) {
		free(im_fanout);
		return GIT_ERROR;
	}

	p->idx_search = idxv2_search;
	p->idx_search_offset = idxv2_search_offset;
	p->idx_get = idxv2_get;
	p->im_fanout = im_fanout;
	p->im_oid = (unsigned char *)(src_fanout + 256);
	p->im_crc = (uint32_t *)(p->im_oid + 20 * p->obj_cnt);
	p->im_offset32 = p->im_crc + p->obj_cnt;
	p->im_offset64 = p->im_offset32 + p->obj_cnt;

	if ((info = git__malloc(sizeof(*info) * (p->obj_cnt+1))) == NULL) {
		free(im_fanout);
		return GIT_ERROR;
	}

	/* check 64-bit offset table index values are within bounds */
	o64_sz = p->idx_map.len - sz;
	o64_len = o64_sz / 8;
	for (j = 0; j < p->obj_cnt; j++) {
		uint32_t o32 = decode32(p->im_offset32 + j);
		off_t offset = o32;
		if (o32 & 0x80000000) {
			uint32_t o64_idx = (o32 & ~0x80000000);
			if (o64_idx >= o64_len) {
				free(im_fanout);
				free(info);
				return GIT_ERROR;
			}
			offset = decode64(p->im_offset64 + 2*o64_idx);
		}
		info[j].offset = offset;
		info[j].n = j;
	}
	info[p->obj_cnt].offset = p->pack_size - GIT_OID_RAWSZ;
	info[p->obj_cnt].n = p->obj_cnt;

	if (make_offset_index(p, info)) {
		free(im_fanout);
		free(info);
		return GIT_ERROR;
	}
	free(info);

	return GIT_SUCCESS;
}








/***********************************************************
 *
 * PACKFILE READING FUNCTIONS
 *
 * Read the contents of a packfile
 *
 ***********************************************************/


static int unpack_object(git_rawobj *out, git_pack *p, index_entry *e);

static int unpack_object_delta(git_rawobj *out, git_pack *p,
		index_entry *base_entry,
		uint8_t *delta_buffer,
		size_t delta_deflated_size,
		size_t delta_inflated_size)
{
	int res = 0;
	uint8_t *delta = NULL;
	git_rawobj base_obj;

	base_obj.data = NULL;
	base_obj.type = GIT_OBJ_BAD;
	base_obj.len = 0;

	if ((res = unpack_object(&base_obj, p, base_entry)) < 0)
		goto cleanup;

	delta = git__malloc(delta_inflated_size + 1);

	if ((res = git_odb__inflate_buffer(delta_buffer, delta_deflated_size,
			delta, delta_inflated_size)) < 0)
		goto cleanup;

	res = git__delta_apply(out, base_obj.data, base_obj.len, delta, delta_inflated_size);

	out->type = base_obj.type;

cleanup:
	free(delta);
	git_rawobj_close(&base_obj);
	return res;
}

static int unpack_object(git_rawobj *out, git_pack *p, index_entry *e)
{
	git_otype object_type;
	size_t inflated_size, deflated_size, shift;
	uint8_t *buffer, byte;

	assert(out && p && e && git__is_sizet(e->size));

	if (open_pack(p))
		return GIT_ERROR;

	buffer = (uint8_t *)p->pack_map.data + e->offset;
	deflated_size = (size_t)e->size;

	if (deflated_size == 0)
		deflated_size = (size_t)(p->pack_size - e->offset);

	byte = *buffer++ & 0xFF;
	deflated_size--;
	object_type = (byte >> 4) & 0x7;
	inflated_size = byte & 0xF;
	shift = 4;

	while (byte & 0x80) {
		byte = *buffer++ & 0xFF;
		deflated_size--;
		inflated_size += (byte & 0x7F) << shift;
		shift += 7;
	}

	switch (object_type) {
		case GIT_OBJ_COMMIT:
		case GIT_OBJ_TREE:
		case GIT_OBJ_BLOB:
		case GIT_OBJ_TAG: {

			/* Handle a normal zlib stream */
			out->len = inflated_size;
			out->type = object_type;
			out->data = git__malloc(inflated_size + 1);

			if (git_odb__inflate_buffer(buffer, deflated_size, out->data, out->len) < 0) {
				free(out->data);
				out->data = NULL;
				return GIT_ERROR;
			}

			return GIT_SUCCESS;
		}

		case GIT_OBJ_OFS_DELTA: {

			off_t delta_offset;
			index_entry entry;

			byte = *buffer++ & 0xFF;
			delta_offset = byte & 0x7F;

			while (byte & 0x80) {
				delta_offset += 1;
				byte = *buffer++ & 0xFF;
				delta_offset <<= 7;
				delta_offset += (byte & 0x7F);
			}

			entry.n = 0;
			entry.oid = NULL;
			entry.offset = e->offset - delta_offset;
			entry.size = 0;

			if (unpack_object_delta(out, p, &entry,
					buffer, deflated_size, inflated_size) < 0)
				return GIT_ERROR;

			return GIT_SUCCESS;
		}

		case GIT_OBJ_REF_DELTA: {

			git_oid base_id;
			uint32_t n;
			index_entry entry;
			int res = GIT_ERROR;

			git_oid_mkraw(&base_id, buffer);

			if (!p->idx_search(&n, p, &base_id) &&
				!p->idx_get(&entry, p, n)) {

				res = unpack_object_delta(out, p, &entry,
					buffer + GIT_OID_RAWSZ, deflated_size, inflated_size);
			}

			return res;
		}

		default:
			return GIT_EOBJCORRUPTED;
	}
}

static int read_packed(git_rawobj *out, const pack_location *loc)
{
	index_entry e;
	int res;

	assert(out && loc);

	if (pack_openidx(loc->ptr) < 0)
		return GIT_EPACKCORRUPTED;

	res = loc->ptr->idx_get(&e, loc->ptr, loc->n);

	if (!res)
		res = unpack_object(out, loc->ptr, &e);

	pack_decidx(loc->ptr);

	return res;
}

static int read_header_packed(git_rawobj *out, const pack_location *loc)
{
	git_pack *pack;
	index_entry e;
	int error = GIT_SUCCESS, shift;
	uint8_t *buffer, byte;

	assert(out && loc);

	pack = loc->ptr;

	if (pack_openidx(pack))
		return GIT_EPACKCORRUPTED;

	if (pack->idx_get(&e, pack, loc->n) < 0 ||
		open_pack(pack) < 0) {
		error = GIT_ENOTFOUND;
		goto cleanup;
	}

	buffer = (uint8_t *)pack->pack_map.data + e.offset;

	byte = *buffer++ & 0xFF;
	out->type = (byte >> 4) & 0x7;
	out->len = byte & 0xF;
	shift = 4;

	while (byte & 0x80) {
		byte = *buffer++ & 0xFF;
		out->len += (byte & 0x7F) << shift;
		shift += 7;
	}

	/*
	 * FIXME: if the object is not packed as a whole,
	 * we need to do a full load and apply the deltas before
	 * being able to read the header.
	 *
	 * I don't think there are any workarounds for this.'
	 */

	if (out->type == GIT_OBJ_OFS_DELTA || out->type == GIT_OBJ_REF_DELTA) {
		error = unpack_object(out, pack, &e);
		git_rawobj_close(out);
	}

cleanup:
	pack_decidx(loc->ptr);
	return error;
}







/***********************************************************
 *
 * PACKED BACKEND PUBLIC API
 *
 * Implement the git_odb_backend API calls
 *
 ***********************************************************/

int pack_backend__read_header(git_rawobj *obj, git_odb_backend *backend, const git_oid *oid)
{
	pack_location location;

	assert(obj && backend && oid);

	if (locate_packfile(&location, (pack_backend *)backend, oid) < 0)
		return GIT_ENOTFOUND;

	return read_header_packed(obj, &location);
}

int pack_backend__read(git_rawobj *obj, git_odb_backend *backend, const git_oid *oid)
{
	pack_location location;

	assert(obj && backend && oid);

	if (locate_packfile(&location, (pack_backend *)backend, oid) < 0)
		return GIT_ENOTFOUND;

	return read_packed(obj, &location);
}

int pack_backend__exists(git_odb_backend *backend, const git_oid *oid)
{
	pack_location location;
	assert(backend && oid);
	return locate_packfile(&location, (pack_backend *)backend, oid) == GIT_SUCCESS;
}

void pack_backend__free(git_odb_backend *_backend)
{
	pack_backend *backend;
	git_packlist *pl;

	assert(_backend);

	backend = (pack_backend *)_backend;

	gitlck_lock(&backend->lock);

	pl = backend->packlist;
	backend->packlist = NULL;

	gitlck_unlock(&backend->lock);
	if (pl)
		packlist_dec(backend, pl);

	gitlck_free(&backend->lock);

	free(backend->objects_dir);
	free(backend);
}

int git_odb_backend_pack(git_odb_backend **backend_out, const char *objects_dir)
{
	pack_backend *backend;

	backend = git__calloc(1, sizeof(pack_backend));
	if (backend == NULL)
		return GIT_ENOMEM;

	backend->objects_dir = git__strdup(objects_dir);
	if (backend->objects_dir == NULL) {
		free(backend);
		return GIT_ENOMEM;
	}

	gitlck_init(&backend->lock);

	backend->parent.read = &pack_backend__read;
	backend->parent.read_header = &pack_backend__read_header;
	backend->parent.write = NULL;
	backend->parent.exists = &pack_backend__exists;
	backend->parent.free = &pack_backend__free;

	backend->parent.priority = 1;

	*backend_out = (git_odb_backend *)backend;
	return GIT_SUCCESS;
}
