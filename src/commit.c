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

#include <time.h>

#include "common.h"
#include "commit.h"
#include "revwalk.h"
#include "git/odb.h"

const git_oid *git_commit_id(git_commit *c)
{
	return &c->id;
}

git_commit *git_commit_lookup(git_revpool *pool, const git_oid *id)
{
    git_obj commit_obj;
    git_commit *commit = NULL;

    /*
     * TODO: check if the commit is already cached in the
     * revpool instead of loading it from the odb
     */

    if (git_odb_read(&commit_obj, pool->db, id) < 0)
        return NULL;

    if (commit_obj.type != GIT_OBJ_COMMIT)
        goto error_cleanup;

    commit = git__malloc(sizeof(git_commit));
    memset(commit, 0x0, sizeof(git_commit));

    git_oid_cpy(&commit->id, id);
    commit->pool = pool;

    if (git_commit__parse_buffer(commit, commit_obj.data, commit_obj.len) < 0)
        goto error_cleanup;

    return commit;

error_cleanup:
    git_obj_close(&commit_obj);
    free(commit);

    return NULL;
}

int git_commit__parse_time(time_t *commit_time, char *buffer, const char *buffer_end)
{
	if (memcmp(buffer, "author ", 7) != 0)
		return -1;

    buffer = memchr(buffer, '\n', buffer_end - buffer);
    if (buffer == 0 || buffer >= buffer_end)
        return -1;

	if (memcmp(buffer, "committer ", 10) != 0)
		return -1;

    buffer = memchr(buffer, '\n', buffer_end - buffer);
    if (buffer == 0 || buffer >= buffer_end)
        return -1;

	*commit_time = strtol(buffer, &buffer, 10);

    return (buffer < buffer_end) ? 0 : -1;
}

int git_commit__parse_oid(git_oid *oid, char **buffer_out, const char *buffer_end, const char *header)
{
    size_t sha_len = GIT_OID_HEXSZ;
    size_t header_len = strlen(header);

    char *buffer = *buffer_out;

    if (buffer + (header_len + sha_len + 1) > buffer_end)
        return -1;

    if (memcmp(buffer, header, header_len) != 0)
        return -1;

    if (buffer[header_len + sha_len] != '\n')
        return -1;

    if (git_oid_mkstr(oid, buffer + header_len) < 0)
        return -1;

    *buffer_out = buffer + (header_len + sha_len + 1);

    return 0;
}

int git_commit__parse_buffer(git_commit *commit, void *data, size_t len)
{
	char *buffer = (char *)data;
	const char *buffer_end = (char *)data + len;

    git_oid oid;

    if (commit->parsed)
		return 0;

    if (git_commit__parse_oid(&oid, &buffer, buffer_end, "tree ") < 0)
        return -1;

    /*
     * TODO: load tree into commit object
     * TODO: commit grafts!
     */

    while (git_commit__parse_oid(&oid, &buffer, buffer_end, "parent ") == 0) {
        git_commit *parent;

        if ((parent = git_commit_lookup(commit->pool, &oid)) == NULL)
            return -1;

        // TODO: push the new commit into the revpool
    }

    if (git_commit__parse_time(&commit->commit_time, buffer, buffer_end) < 0)
        return -1;

    commit->parsed = 1;

	return 0;
}
