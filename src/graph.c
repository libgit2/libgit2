
/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "revwalk.h"
#include "merge.h"
#include "git2/graph.h"

static int ahead_behind(git_commit_list_node *one, git_commit_list_node *two,
	size_t *ahead, size_t *behind)
{
	git_commit_list_node *commit;
	git_pqueue pq;
	int i;
	*ahead = 0;
	*behind = 0;

	if (git_pqueue_init(&pq, 2, git_commit_list_time_cmp) < 0)
		return -1;
	if (git_pqueue_insert(&pq, one) < 0)
		goto on_error;
	if (git_pqueue_insert(&pq, two) < 0)
		goto on_error;

	while ((commit = git_pqueue_pop(&pq)) != NULL) {
		if (commit->flags & RESULT ||
			(commit->flags & (PARENT1 | PARENT2)) == (PARENT1 | PARENT2))
			continue;
		else if (commit->flags & PARENT1)
			(*behind)++;
		else if (commit->flags & PARENT2)
			(*ahead)++;

		for (i = 0; i < commit->out_degree; i++) {
			git_commit_list_node *p = commit->parents[i];
			if (git_pqueue_insert(&pq, p) < 0)
				return -1;
		}
		commit->flags |= RESULT;
	}

	git_pqueue_free(&pq);
	return 0;

on_error:
	git_pqueue_free(&pq);
	return -1;
}

int git_graph_ahead_behind(size_t *ahead, size_t *behind, git_repository *repo,
	const git_oid *one, const git_oid *two)
{
	git_revwalk *walk;
	git_vector list;
	struct git_commit_list *result = NULL;
	git_commit_list_node *commit1, *commit2;
	void *contents[1];

	if (git_revwalk_new(&walk, repo) < 0)
		return -1;

	commit2 = git_revwalk__commit_lookup(walk, two);
	if (commit2 == NULL)
		goto on_error;

	/* This is just one value, so we can do it on the stack */
	memset(&list, 0x0, sizeof(git_vector));
	contents[0] = commit2;
	list.length = 1;
	list.contents = contents;

	commit1 = git_revwalk__commit_lookup(walk, one);
	if (commit1 == NULL)
		goto on_error;

	if (git_merge__bases_many(&result, walk, commit1, &list) < 0)
		goto on_error;
	if (ahead_behind(commit1, commit2, ahead, behind) < 0)
		goto on_error;

	if (!result) {
		git_revwalk_free(walk);
		return GIT_ENOTFOUND;
	}

	git_commit_list_free(&result);
	git_revwalk_free(walk);

	return 0;

on_error:
	git_revwalk_free(walk);
	return -1;
}
