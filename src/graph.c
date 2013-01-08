
/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "revwalk.h"
#include "merge.h"
#include "git2/graph.h"

static int interesting(git_pqueue *list, git_commit_list *roots)
{
	unsigned int i;
	/* element 0 isn't used - we need to start at 1 */
	for (i = 1; i < list->size; i++) {
		git_commit_list_node *commit = list->d[i];
		if ((commit->flags & STALE) == 0)
			return 1;
	}

	while(roots) {
		if ((roots->item->flags & STALE) == 0)
			return 1;
		roots = roots->next;
	}

	return 0;
}

static int mark_parents(git_revwalk *walk, git_commit_list_node *one,
	git_commit_list_node *two)
{
	unsigned int i;
	git_commit_list *roots = NULL;
	git_pqueue list;

	/* if the commit is repeated, we have a our merge base already */
	if (one == two) {
		one->flags |= PARENT1 | PARENT2 | RESULT;
		return 0;
	}

	if (git_pqueue_init(&list, 2, git_commit_list_time_cmp) < 0)
		return -1;

	if (git_commit_list_parse(walk, one) < 0)
		goto on_error;
	one->flags |= PARENT1;
	if (git_pqueue_insert(&list, one) < 0)
		goto on_error;

	if (git_commit_list_parse(walk, two) < 0)
		goto on_error;
	two->flags |= PARENT2;
	if (git_pqueue_insert(&list, two) < 0)
		goto on_error;

	/* as long as there are non-STALE commits */
	while (interesting(&list, roots)) {
		git_commit_list_node *commit;
		int flags;

		commit = git_pqueue_pop(&list);
		if (commit == NULL)
			break;

		flags = commit->flags & (PARENT1 | PARENT2 | STALE);
		if (flags == (PARENT1 | PARENT2)) {
			if (!(commit->flags & RESULT))
				commit->flags |= RESULT;
			/* we mark the parents of a merge stale */
			flags |= STALE;
		}

		for (i = 0; i < commit->out_degree; i++) {
			git_commit_list_node *p = commit->parents[i];
			if ((p->flags & flags) == flags)
				continue;

			if (git_commit_list_parse(walk, p) < 0)
				goto on_error;

			p->flags |= flags;
			if (git_pqueue_insert(&list, p) < 0)
				goto on_error;
		}

		/* Keep track of root commits, to make sure the path gets marked */
		if (commit->out_degree == 0) {
			if (git_commit_list_insert(commit, &roots) == NULL)
				goto on_error;
		}
	}

	git_commit_list_free(&roots);
	git_pqueue_free(&list);
	return 0;

on_error:
	git_commit_list_free(&roots);
	git_pqueue_free(&list);
	return -1;
}


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
	git_commit_list_node *commit1, *commit2;

	if (git_revwalk_new(&walk, repo) < 0)
		return -1;

	commit2 = git_revwalk__commit_lookup(walk, two);
	if (commit2 == NULL)
		goto on_error;

	commit1 = git_revwalk__commit_lookup(walk, one);
	if (commit1 == NULL)
		goto on_error;

	if (mark_parents(walk, commit1, commit2) < 0)
		goto on_error;
	if (ahead_behind(commit1, commit2, ahead, behind) < 0)
		goto on_error;

	git_revwalk_free(walk);

	return 0;

on_error:
	git_revwalk_free(walk);
	return -1;
}
