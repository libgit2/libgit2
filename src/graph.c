
/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "revwalk.h"
#include "merge.h"
#include "git2/graph.h"
#include "graph.h"

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

static int graph_commit_time_cmp(const void *a, const void *b)
{
	const git_graph_commit *ca = (const git_graph_commit *)(a);
	const git_graph_commit *cb = (const git_graph_commit *)(b);

	return cb->time - ca->time;
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

static int insert_graph_commit(git_graph_commit_list *list,
	git_commit_list_node *node)
{
	git_graph_commit *commit = git__calloc(1, sizeof(git_graph_commit));
	GITERR_CHECK_ALLOC(commit);
	
	commit->oid = node->oid;
	commit->time = node->time;

	git_vector_insert(&list->commits, commit);

	return 0;
}

static int ahead_behind(git_commit_list_node *one, git_commit_list_node *two,
	git_graph_commit_list **ahead_out, git_graph_commit_list **behind_out)
{
	git_commit_list_node *commit;
	git_pqueue pq;
	int i;
	git_graph_commit_list *ahead = NULL, *behind = NULL;

	assert(ahead_out);
	assert(behind_out);
	memset(&pq, 0, sizeof(git_pqueue));

	ahead = git__calloc(1, sizeof(git_graph_commit_list));
	if (ahead == NULL)
		goto on_error;
	behind = git__calloc(1, sizeof(git_graph_commit_list));
	if (behind == NULL)
		goto on_error;

	if (git_vector_init(&ahead->commits, 8, graph_commit_time_cmp) < 0 ||
		git_vector_init(&behind->commits, 8, graph_commit_time_cmp) < 0)
		goto on_error;

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
		else if (commit->flags & PARENT1) {
			if (insert_graph_commit(behind, commit) < 0)
				goto on_error;
		}
		else if (commit->flags & PARENT2) {
			if (insert_graph_commit(ahead, commit) < 0)
				goto on_error;
		}

		for (i = 0; i < commit->out_degree; i++) {
			git_commit_list_node *p = commit->parents[i];
			if (git_pqueue_insert(&pq, p) < 0)
				return -1;
		}
		commit->flags |= RESULT;
	}

	git_vector_sort(&ahead->commits);
	git_vector_sort(&behind->commits);

	*ahead_out = ahead;
	*behind_out = behind;

	git_pqueue_free(&pq);
	return 0;

on_error:
	git_pqueue_free(&pq);
	git_graph_commit_list_free(ahead);
	git_graph_commit_list_free(behind);
	return -1;
}

const git_oid *git_graph_commit_list_get_byindex(
	git_graph_commit_list *list,
	size_t pos)
{
	assert(list);
	return git_vector_get(&list->commits, pos);
}

int git_graph_commit_list_count(git_graph_commit_list *list)
{
	assert(list);
	return list->commits.length;
}

void git_graph_commit_list_free(git_graph_commit_list *list)
{
	size_t i;
	git_graph_commit *commit;

	if (list == NULL)
		return;

	for(i = 0; i < list->commits.length; ++i){
		commit = git_vector_get(&list->commits, i);
		git__free(commit);
	}

	git_vector_free(&list->commits);
	git__free(list);
}

int git_graph_ahead_behind(
	git_graph_commit_list **ahead,
	git_graph_commit_list **behind,
	git_repository *repo,
	const git_oid *local,
	const git_oid *upstream)
{
	git_revwalk *walk;
	git_commit_list_node *commit_u, *commit_l;

	if (git_revwalk_new(&walk, repo) < 0)
		return -1;

	commit_u = git_revwalk__commit_lookup(walk, upstream);
	if (commit_u == NULL)
		goto on_error;

	commit_l = git_revwalk__commit_lookup(walk, local);
	if (commit_l == NULL)
		goto on_error;

	if (mark_parents(walk, commit_l, commit_u) < 0)
		goto on_error;
	if (ahead_behind(commit_l, commit_u, ahead, behind) < 0)
		goto on_error;

	git_revwalk_free(walk);

	return 0;

on_error:
	git_revwalk_free(walk);
	return -1;
}
