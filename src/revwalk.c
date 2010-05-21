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
#include "commit.h"
#include "revwalk.h"

git_revpool *gitrp_alloc(git_odb *db)
{
	git_revpool *walk = git__malloc(sizeof(*walk));
	if (!walk)
		return NULL;

    memset(walk, 0x0, sizeof(git_revpool));

	walk->db = db;
	return walk;
}

void gitrp_free(git_revpool *walk)
{
    git_commit_list *list;

    list = walk->commits;
    while (list)
    {
        free(list->commit);
        list = list->next;
    }

	free(walk);
}

void gitrp_push(git_revpool *pool, git_commit *commit)
{
    if (commit->pool != pool)
        return;

    if ((commit->flags & GIT_COMMIT_SEEN) != 0)
        return;

    if (!commit->parsed)
    {
        if (git_commit_parse_existing(commit) < 0)
            return;
    }

    commit->flags |= GIT_COMMIT_SEEN;

    git_commit_list_insert(&pool->roots, commit);
}

void gitrp_prepare_walk(git_revpool *pool)
{
    // TODO: sort commit list based on walk ordering

    pool->iterator = pool->roots;
    pool->walking = 1;
}

git_commit *gitrp_next(git_revpool *pool)
{
    git_commit *next;

    if (!pool->walking)
        gitrp_prepare_walk(pool);

    // Iteration finished
    if (pool->iterator == NULL)
    {
        gitrp_reset(pool);
        return NULL;
    }

    next = pool->iterator->commit;
    pool->iterator = pool->iterator->next;

    return next;
}

void gitrp_reset(git_revpool *pool)
{
    pool->iterator = NULL;
    pool->walking = 0;
}
