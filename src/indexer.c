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

#include "git2/indexer.h"
#include "git2/object.h"
#include "git2/zlib.h"

#include "common.h"
#include "pack.h"
#include "mwindow.h"
#include "posix.h"

typedef struct git_indexer {
	struct pack_file *pack;
	git_vector objects;
	git_vector deltas;
	struct stat st;
	git_indexer_stats stats;
} git_indexer;

static int parse_header(git_indexer *idx)
{
	struct pack_header hdr;
	int error;

	/* Verify we recognize this pack file format. */
	if ((error = p_read(idx->pack->mwf.fd, &hdr, sizeof(hdr))) < GIT_SUCCESS)
		goto cleanup;

	if (hdr.hdr_signature != htonl(PACK_SIGNATURE)) {
		error = git__throw(GIT_EOBJCORRUPTED, "Wrong pack signature");
		goto cleanup;
	}

	if (!pack_version_ok(hdr.hdr_version)) {
		error = git__throw(GIT_EOBJCORRUPTED, "Wrong pack version");
		goto cleanup;
	}

	/*
	 * FIXME: At this point we have no idea how many of the are
	 * deltas, so assume all objects are both until we get a better
	 * idea 
	 */
	error = git_vector_init(&idx->objects, hdr.hdr_entries, NULL /* FIXME: probably need something */);
	if (error < GIT_SUCCESS)
		goto cleanup;

	error = git_vector_init(&idx->deltas, hdr.hdr_entries, NULL /* FIXME: probably need something */);
	if (error < GIT_SUCCESS)
		goto cleanup;

	idx->stats.total = hdr.hdr_entries;

	return GIT_SUCCESS;

cleanup:
	git_vector_free(&idx->objects);
	git_vector_free(&idx->deltas);

	return error;
}

int git_indexer_new(git_indexer **out, const char *packname)
{
	git_indexer *idx;
	unsigned int namelen;
	int ret, error;

	idx = git__malloc(sizeof(git_indexer));
	if (idx == NULL)
		return GIT_ENOMEM;

	memset(idx, 0x0, sizeof(*idx));

	namelen = strlen(packname);
	idx->pack = git__malloc(sizeof(struct pack_file) + namelen + 1);
	if (idx->pack == NULL)
		goto cleanup;

	memset(idx->pack, 0x0, sizeof(struct pack_file));
	memcpy(idx->pack->pack_name, packname, namelen);

	ret = p_stat(packname, &idx->st);
	if (ret < 0) {
		if (errno == ENOENT)
			error = git__throw(GIT_ENOTFOUND, "Failed to stat packfile. File not found");
		else
			error = git__throw(GIT_EOSERR, "Failed to stat packfile.");

		goto cleanup;
	}

	ret = p_open(idx->pack->pack_name, O_RDONLY);
	if (ret < 0) {
		error = git__throw(GIT_EOSERR, "Failed to open packfile");
		goto cleanup;
	}

	idx->pack->mwf.fd = ret;

	error = parse_header(idx);
	if (error < GIT_SUCCESS) {
		error = git__rethrow(error, "Failed to parse packfile header");
		goto cleanup;
	}

	*out = idx;

	return GIT_SUCCESS;

cleanup:
	free(idx->pack);
	free(idx);

	return error;
}

/*
 * Parse the variable-width length and return it. Assumes that the
 * whole number exists inside the buffer. As this is the git format,
 * the first byte only contains length information in the lower nibble
 * because the higher one is used for type and continuation. The
 * output parameter is necessary because we don't know how long the
 * entry is actually going to be.
 */
static unsigned long entry_len(const char **bufout, const char *buf)
{
	unsigned long size, c;
	const char *p = buf;
	unsigned shift;

	c = *p;
	size = c & 0xf;
	shift = 4;

	/* As long as the MSB is set, we need to continue */
	while (c & 0x80) {
		p++;
		c = *p;
		size += (c & 0x7f) << shift;
		shift += 7;
	}

	*bufout = p;
	return size;
}

static git_otype entry_type(const char *buf)
{
	return (*buf >> 4) & 7;
}

/*
 * Create the index. Every time something interesting happens
 * (something has been parse or resolved), the callback gets called
 * with some stats so it can tell the user how hard we're working
 */
int git_indexer_run(git_indexer *idx, int (*cb)(const git_indexer_stats *, void *), void *cb_data)
{
	git_mwindow_file *mwf = &idx->pack->mwf;
	git_mwindow *w = NULL;
	off_t off = 0;
	int error;
	const char *ptr;
	unsigned int fanout[256] = {0};

	error = git_mwindow_file_register(mwf);
	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to register mwindow file");

	/* Notify before the first one */
	if (cb)
		cb(&idx->stats, cb_data);

	while (idx->stats.processed < idx->stats.total) {
		size_t size;
		git_otype type;

		error = git_packfile_unpack_header(&size, &type, mwf, &w, &off);

		switch (type) {
		case GIT_OBJ_COMMIT:
		case GIT_OBJ_TREE:
		case GIT_OBJ_BLOB:
		case GIT_OBJ_TAG:
			break;
		default:
			error = git__throw(GIT_EOBJCORRUPTED, "Invalid object type");
			goto cleanup;
		}

		/*
		 * Do we need to uncompress everything if we're not running in
		 * strict mode? Or at least can't we free the data?
		 */

		/* Get a window for the compressed data */
		//ptr = git_mwindow_open(mwf, &w, idx->pack->pack_fd, size, data - ptr, 0, NULL);

		idx->stats.processed++;

		if (cb)
			cb(&idx->stats, cb_data);

	}

cleanup:
	git_mwindow_free_all(mwf);

	return error;

}

void git_indexer_free(git_indexer *idx)
{
	p_close(idx->pack->mwf.fd);
	git_vector_free(&idx->objects);
	git_vector_free(&idx->deltas);
	free(idx->pack);
	free(idx);
}

