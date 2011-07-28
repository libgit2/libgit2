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

struct entry {
	unsigned char sha[GIT_OID_RAWSZ];
	uint32_t crc;
	uint32_t offset;
	uint64_t offset_long;
};

typedef struct git_indexer {
	struct git_pack_file *pack;
	struct stat st;
	git_indexer_stats stats;
	struct git_pack_header hdr;
	struct entry *objects;
} git_indexer;

static int parse_header(git_indexer *idx)
{
	int error;

	/* Verify we recognize this pack file format. */
	if ((error = p_read(idx->pack->mwf.fd, &idx->hdr, sizeof(idx->hdr))) < GIT_SUCCESS)
		return git__rethrow(error, "Failed to read in pack header");

	if (idx->hdr.hdr_signature != htonl(PACK_SIGNATURE))
		return git__throw(GIT_EOBJCORRUPTED, "Wrong pack signature");

	if (!pack_version_ok(idx->hdr.hdr_version))
		return git__throw(GIT_EOBJCORRUPTED, "Wrong pack version");


	return GIT_SUCCESS;
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
	idx->pack = git__malloc(sizeof(struct git_pack_file) + namelen + 1);
	if (idx->pack == NULL)
		goto cleanup;

	memset(idx->pack, 0x0, sizeof(struct git_pack_file));
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

	idx->objects = git__calloc(sizeof(struct entry), idx->hdr.hdr_entries);
	if (idx->objects == NULL) {
		error = GIT_ENOMEM;
		goto cleanup;
	}

	idx->stats.total = idx->hdr.hdr_entries;

	*out = idx;

	return GIT_SUCCESS;

cleanup:
	free(idx->pack);
	free(idx);

	return error;
}

/*
 * Create the index. Every time something interesting happens
 * (something has been parse or resolved), the callback gets called
 * with some stats so it can tell the user how hard we're working
 */
int git_indexer_run(git_indexer *idx, int (*cb)(const git_indexer_stats *, void *), void *cb_data)
{
	git_mwindow_file *mwf = &idx->pack->mwf;
	off_t off = 0;
	int error;
	unsigned int fanout[256] = {0};

	/* FIXME: Write the keep file */

	error = git_mwindow_file_register(mwf);
	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to register mwindow file");

	/* Notify before the first one */
	if (cb)
		cb(&idx->stats, cb_data);

	while (idx->stats.processed < idx->stats.total) {
		git_rawobj obj;
		git_oid oid;
		struct entry entry;
		char hdr[512] = {0}; /* FIXME: How long should this be? */
		int i, hdr_len;

		memset(&entry, 0x0, sizeof(entry)); /* Necessary? */

		if (off > UINT31_MAX) {
			entry.offset = ~0ULL;
			entry.offset_long = off;
		} else {
			entry.offset = off;
		}

		error = git_packfile_unpack(&obj, idx->pack, &off);
		if (error < GIT_SUCCESS) {
			error = git__rethrow(error, "Failed to unpack object");
			goto cleanup;
		}

		error = git_odb__hash_obj(&oid, hdr, sizeof(hdr), &hdr_len, &obj);
		if (error < GIT_SUCCESS) {
			error = git__rethrow(error, "Failed to hash object");
			goto cleanup;
		}

		memcpy(&entry.sha, oid.id, GIT_OID_RAWSZ);
		/* entry.crc = crc32(obj.data) */

		/* Add the object to the list */
		//memcpy(&idx->objects[idx->stats.processed], &entry, sizeof(entry));
		idx->objects[idx->stats.processed] = entry;

		for (i = oid.id[0]; i < 256; ++i) {
			fanout[i]++;
		}

		free(obj.data);

		idx->stats.processed++;

		if (cb)
			cb(&idx->stats, cb_data);

	}

	/*
	 * All's gone well, so let's write the index file.
	 */

cleanup:
	git_mwindow_free_all(mwf);

	return error;

}

void git_indexer_free(git_indexer *idx)
{
	p_close(idx->pack->mwf.fd);
	free(idx->objects);
	free(idx->pack);
	free(idx);
}

