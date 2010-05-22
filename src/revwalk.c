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

    list = walk->roots;
    while (list)
    {
        free(list->commit);
        free(list);

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

    // Sanity check: make sure that if the commit
    // has been manually marked as uninteresting,
    // all the parent commits are too.
    if ((commit->flags & GIT_COMMIT_HIDE) != 0)
        git_commit__mark_uninteresting(commit);

    commit->flags |= GIT_COMMIT_SEEN;

    git_commit_list_insert(&pool->roots, commit);
    git_commit_list_insert(&pool->iterator, commit);
}

void gitrp_hide(git_revpool *pool, git_commit *commit)
{
    git_commit_mark_uninteresting(commit);
    gitrp_push(pool, commit);
}

void gitrp_prepare_walk(git_revpool *pool)
{
    git_commit_list *list;

    list = pool->roots;
    while (list)
    {
        git_commit_list_insert(&pool->iterator, list->commit);
        list = list->next;
    }

    pool->walking = 1;
}

git_commit *gitrp_next(git_revpool *pool)
{
    git_commit *next;

    if (!pool->walking)
        gitrp_prepare_walk(pool);

    while (pool->iterator != NULL)
    {
        git_commit_list *list;

        next = pool->iterator->commit;
        free(pool->iterator);
        pool->iterator = pool->iterator->next;

        list = next->parents;
        while (list)
        {
            git_commit *parent = list->commit;
            list = list->next;

            if ((parent->flags & GIT_COMMIT_SEEN) != 0)
                continue;

            if (parent->parsed == 0)
                git_commit_parse_existing(parent);

            git_commit_list_insert(&pool->iterator, list->commit);
        }

        if ((next->flags & GIT_COMMIT_HIDE) != 0)
            return next;
    }

    // No commits left to iterate
    gitrp_reset(pool);
    return NULL;
}

void gitrp_reset(git_revpool *pool)
{
    pool->iterator = NULL;
    pool->walking = 0;
}
