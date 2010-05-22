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
    git_commit_list_clear(&(walk->iterator), 0);
    git_commit_list_clear(&(walk->roots), 1);
	free(walk);
}

void gitrp_push(git_revpool *pool, git_commit *commit)
{
    if (commit->pool != pool)
        return;

    if (commit->seen)
        return;

    if (!commit->parsed)
    {
        if (git_commit_parse_existing(commit) < 0)
            return;
    }

    // Sanity check: make sure that if the commit
    // has been manually marked as uninteresting,
    // all the parent commits are too.
    if (commit->uninteresting)
        git_commit__mark_uninteresting(commit);

    commit->seen = 1;

    git_commit_list_append(&pool->roots, commit);
    git_commit_list_append(&pool->iterator, commit);
}

void gitrp_hide(git_revpool *pool, git_commit *commit)
{
    git_commit__mark_uninteresting(commit);
    gitrp_push(pool, commit);
}

void gitrp_prepare_walk(git_revpool *pool)
{
    git_commit_node *roots;

    roots = pool->roots.head;
    while (roots)
    {
        git_commit_list_append(&pool->iterator, roots->commit);
        roots = roots->next;
    }

    pool->walking = 1;
}

git_commit *gitrp_next(git_revpool *pool)
{
    git_commit *next;

    if (!pool->walking)
        gitrp_prepare_walk(pool);

    while ((next = git_commit_list_pop_front(&pool->iterator)) != NULL)
    {
        git_commit_node *parents;

        parents = next->parents.head;
        while (parents)
        {
            git_commit *parent = parents->commit;
            parents = parents->next;

            if (parent->seen)
                continue;

            if (parent->parsed == 0)
                git_commit_parse_existing(parent);

            git_commit_list_append(&pool->iterator, parent);
        }

        if (next->uninteresting == 0)
            return next;
    }

    // No commits left to iterate
    gitrp_reset(pool);
    return NULL;
}

void gitrp_reset(git_revpool *pool)
{
    git_commit_list_clear(&pool->iterator, 0);
    pool->walking = 0;
}

