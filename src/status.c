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
#include "git2.h"
#include "fileops.h"
#include "hash.h"

int git_status_hashfile(git_oid *out, const char *path)
{
	int fd, len;
	char hdr[64], buffer[2048];
	git_off_t size;
	git_hash_ctx *ctx;

	if ((fd = p_open(path, O_RDONLY)) < 0)
		return git__throw(GIT_ENOTFOUND, "Could not open '%s'", path);

	if ((size = git_futils_filesize(fd)) < 0 || !git__is_sizet(size)) {
		p_close(fd);
		return git__throw(GIT_EOSERR, "'%s' appears to be corrupted", path);
	}

	ctx = git_hash_new_ctx();

	len = snprintf(hdr, sizeof(hdr), "blob %"PRIuZ, (size_t)size);
	assert(len > 0);
	assert(((size_t) len) < sizeof(hdr));
	if (len < 0 || ((size_t) len) >= sizeof(hdr))
		return git__throw(GIT_ERROR, "Failed to format blob header. Length is out of bounds");

	git_hash_update(ctx, hdr, len+1);

	while (size > 0) {
		ssize_t read_len;

		read_len = read(fd, buffer, sizeof(buffer));

		if (read_len < 0) {
			p_close(fd);
			git_hash_free_ctx(ctx);
			return git__throw(GIT_EOSERR, "Can't read full file '%s'", path);
		}

		git_hash_update(ctx, buffer, read_len);
		size -= read_len;
	}

	p_close(fd);

	git_hash_final(out, ctx);
	git_hash_free_ctx(ctx);

	return GIT_SUCCESS;
}
