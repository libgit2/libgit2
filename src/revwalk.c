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

static const int default_table_size = 32;

git_revpool *gitrp_alloc(git_odb *db)
{
	git_revpool *walk = git__malloc(sizeof(*walk));
	if (!walk)
		return NULL;

	memset(walk, 0x0, sizeof(git_revpool));

	walk->commits = git_revpool_table_create(default_table_size);

	walk->db = db;
	return walk;
}

void gitrp_free(git_revpool *walk)
{
	git_commit_list_clear(&(walk->iterator), 0);
	git_commit_list_clear(&(walk->roots), 0);

	git_revpool_table_free(walk->commits);

	free(walk);
}

void gitrp_sorting(git_revpool *pool, unsigned int sort_mode)
{
	if (pool->walking)
		return;

	pool->sorting = sort_mode;
	gitrp_reset(pool);
}

int gitrp_push(git_revpool *pool, git_commit *commit)
{
	if (commit == NULL || commit->seen)
		return GIT_ENOTFOUND;

	if (commit->object.pool != pool || pool->walking)
		return GIT_ERROR;

	if (!commit->parsed) {
		int error = git_commit_parse_existing(commit);
		if (error < 0)
			return error;
	}

	/*
	 * Sanity check: make sure that if the commit
	 * has been manually marked as uninteresting,
	 * all the parent commits are too.
	 */
	if (commit->uninteresting)
		git_commit__mark_uninteresting(commit);

	if (git_commit_list_push_back(&pool->roots, commit) < 0)
		return GIT_ENOMEM;

	return 0;
}

int gitrp_hide(git_revpool *pool, git_commit *commit)
{
	if (pool->walking)
		return GIT_ERROR;

	git_commit__mark_uninteresting(commit);
	return gitrp_push(pool, commit);
}

int gitrp__enroot(git_revpool *pool, git_commit *commit)
{
	int error;
	git_commit_node *parents;

	if (commit->seen)
		return 0;

	if (commit->parsed == 0) {
		error = git_commit_parse_existing(commit);
		if (error < 0)
			return error;
	}

	commit->seen = 1;

	for (parents = commit->parents.head; parents != NULL; parents = parents->next) {
		parents->commit->in_degree++;

		error = gitrp__enroot(pool, parents->commit);
		if (error < 0)
			return error;
	}

	if (git_commit_list_push_back(&pool->iterator, commit))
		return GIT_ENOMEM;

	return 0;
}

void gitrp__prepare_walk(git_revpool *pool)
{
	git_commit_node *it;

	for (it = pool->roots.head; it != NULL; it = it->next)
		gitrp__enroot(pool, it->commit);

	if (pool->sorting & GIT_RPSORT_TIME)
		git_commit_list_timesort(&pool->iterator);

	if (pool->sorting & GIT_RPSORT_TOPOLOGICAL)
		git_commit_list_toposort(&pool->iterator);

	if (pool->sorting & GIT_RPSORT_REVERSE)
		pool->next_commit = &git_commit_list_pop_back;
	else
		pool->next_commit = &git_commit_list_pop_front;

	pool->walking = 1;
}

git_commit *gitrp_next(git_revpool *pool)
{
	git_commit *next;

	if (!pool->walking)
		gitrp__prepare_walk(pool);

	while ((next = pool->next_commit(&pool->iterator)) != NULL) {
		if (!next->uninteresting)
			return next;
	}

	/* No commits left to iterate */
	gitrp_reset(pool);
	return NULL;
}

void gitrp_reset(git_revpool *pool)
{
	git_commit *commit;
	git_revpool_tableit it;

	git_revpool_tableit_init(pool->commits, &it);

	while ((commit = (git_commit *)git_revpool_tableit_next(&it)) != NULL) {
		commit->seen = 0;
		commit->topo_delay = 0;
		commit->in_degree = 0;
	}

	git_commit_list_clear(&pool->iterator, 0);
	pool->walking = 0;
}

