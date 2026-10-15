/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "revwalk.h"

#include "commit.h"
#include "odb.h"
#include "pathspec.h"
#include "pool.h"

#include "git2/revparse.h"
#include "merge.h"
#include "vector.h"
#include "hashmap_oid.h"

GIT_HASHMAP_OID_FUNCTIONS(git_revwalk_oidmap, GIT_HASHMAP_INLINE, git_commit_list_node *);

static int get_revision(git_commit_list_node **out, git_revwalk *walk, git_commit_list **list);

git_commit_list_node *git_revwalk__commit_lookup(
	git_revwalk *walk, const git_oid *oid)
{
	git_commit_list_node *commit;

	/* lookup and reserve space if not already present */
	if (git_revwalk_oidmap_get(&commit, &walk->commits, oid) == 0)
		return commit;

	commit = git_commit_list_alloc_node(walk);
	if (commit == NULL)
		return NULL;

	git_oid_cpy(&commit->oid, oid);

	if (git_revwalk_oidmap_put(&walk->commits, &commit->oid, commit) < 0)
		return NULL;

	return commit;
}

int git_revwalk__push_commit(git_revwalk *walk, const git_oid *oid, const git_revwalk__push_options *opts)
{
	git_oid commit_id;
	int error;
	git_object *obj, *oobj;
	git_commit_list_node *commit;
	git_commit_list *list;

	if ((error = git_object_lookup(&oobj, walk->repo, oid, GIT_OBJECT_ANY)) < 0)
		return error;

	error = git_object_peel(&obj, oobj, GIT_OBJECT_COMMIT);
	git_object_free(oobj);

	if (error == GIT_ENOTFOUND || error == GIT_EINVALIDSPEC || error == GIT_EPEEL) {
		/* If this comes from e.g. push_glob("tags"), ignore this */
		if (opts->from_glob)
			return 0;

		git_error_set(GIT_ERROR_INVALID, "object is not a committish");
		return error;
	}
	if (error < 0)
		return error;

	git_oid_cpy(&commit_id, git_object_id(obj));
	git_object_free(obj);

	commit = git_revwalk__commit_lookup(walk, &commit_id);
	if (commit == NULL)
		return -1; /* error already reported by failed lookup */

	/* A previous hide already told us we don't want this commit  */
	if (commit->uninteresting)
		return 0;

	if (opts->uninteresting) {
		walk->limited = 1;
		walk->did_hide = 1;
	} else {
		walk->did_push = 1;
	}

	commit->uninteresting = opts->uninteresting;
	list = walk->user_input;

	/* To insert by date, we need to parse so we know the date. */
	if (opts->insert_by_date && ((error = git_commit_list_parse(walk, commit)) < 0))
		return error;

	if ((opts->insert_by_date == 0 ||
	    git_commit_list_insert_by_date(commit, &list) == NULL) &&
	    git_commit_list_insert(commit, &list) == NULL) {
		git_error_set_oom();
		return -1;
	}

	walk->user_input = list;

	return 0;
}

int git_revwalk_push(git_revwalk *walk, const git_oid *oid)
{
	git_revwalk__push_options opts = GIT_REVWALK__PUSH_OPTIONS_INIT;

	GIT_ASSERT_ARG(walk);
	GIT_ASSERT_ARG(oid);

	return git_revwalk__push_commit(walk, oid, &opts);
}


int git_revwalk_hide(git_revwalk *walk, const git_oid *oid)
{
	git_revwalk__push_options opts = GIT_REVWALK__PUSH_OPTIONS_INIT;

	GIT_ASSERT_ARG(walk);
	GIT_ASSERT_ARG(oid);

	opts.uninteresting = 1;
	return git_revwalk__push_commit(walk, oid, &opts);
}

int git_revwalk__push_ref(git_revwalk *walk, const char *refname, const git_revwalk__push_options *opts)
{
	git_oid oid;

	int error = git_reference_name_to_id(&oid, walk->repo, refname);
	if (opts->from_glob && (error == GIT_ENOTFOUND || error == GIT_EINVALIDSPEC || error == GIT_EPEEL)) {
		return 0;
	} else if (error < 0) {
		return -1;
	}

	return git_revwalk__push_commit(walk, &oid, opts);
}

int git_revwalk__push_glob(git_revwalk *walk, const char *glob, const git_revwalk__push_options *given_opts)
{
	git_revwalk__push_options opts = GIT_REVWALK__PUSH_OPTIONS_INIT;
	int error = 0;
	git_str buf = GIT_STR_INIT;
	git_reference *ref;
	git_reference_iterator *iter;
	size_t wildcard;

	GIT_ASSERT_ARG(walk);
	GIT_ASSERT_ARG(glob);

	if (given_opts)
		memcpy(&opts, given_opts, sizeof(opts));

	/* refs/ is implied if not given in the glob */
	if (git__prefixcmp(glob, GIT_REFS_DIR) != 0)
		git_str_joinpath(&buf, GIT_REFS_DIR, glob);
	else
		git_str_puts(&buf, glob);
	GIT_ERROR_CHECK_ALLOC_STR(&buf);

	/* If no '?', '*' or '[' exist, we append '/ *' to the glob */
	wildcard = strcspn(glob, "?*[");
	if (!glob[wildcard])
		git_str_put(&buf, "/*", 2);

	if ((error = git_reference_iterator_glob_new(&iter, walk->repo, buf.ptr)) < 0)
		goto out;

	opts.from_glob = true;
	while ((error = git_reference_next(&ref, iter)) == 0) {
		error = git_revwalk__push_ref(walk, git_reference_name(ref), &opts);
		git_reference_free(ref);
		if (error < 0)
			break;
	}
	git_reference_iterator_free(iter);

	if (error == GIT_ITEROVER)
		error = 0;
out:
	git_str_dispose(&buf);
	return error;
}

int git_revwalk_push_glob(git_revwalk *walk, const char *glob)
{
	git_revwalk__push_options opts = GIT_REVWALK__PUSH_OPTIONS_INIT;

	GIT_ASSERT_ARG(walk);
	GIT_ASSERT_ARG(glob);

	return git_revwalk__push_glob(walk, glob, &opts);
}

int git_revwalk_hide_glob(git_revwalk *walk, const char *glob)
{
	git_revwalk__push_options opts = GIT_REVWALK__PUSH_OPTIONS_INIT;

	GIT_ASSERT_ARG(walk);
	GIT_ASSERT_ARG(glob);

	opts.uninteresting = 1;
	return git_revwalk__push_glob(walk, glob, &opts);
}

int git_revwalk_push_head(git_revwalk *walk)
{
	git_revwalk__push_options opts = GIT_REVWALK__PUSH_OPTIONS_INIT;

	GIT_ASSERT_ARG(walk);

	return git_revwalk__push_ref(walk, GIT_HEAD_REF, &opts);
}

int git_revwalk_hide_head(git_revwalk *walk)
{
	git_revwalk__push_options opts = GIT_REVWALK__PUSH_OPTIONS_INIT;

	GIT_ASSERT_ARG(walk);

	opts.uninteresting = 1;
	return git_revwalk__push_ref(walk, GIT_HEAD_REF, &opts);
}

int git_revwalk_push_ref(git_revwalk *walk, const char *refname)
{
	git_revwalk__push_options opts = GIT_REVWALK__PUSH_OPTIONS_INIT;

	GIT_ASSERT_ARG(walk);
	GIT_ASSERT_ARG(refname);

	return git_revwalk__push_ref(walk, refname, &opts);
}

int git_revwalk_push_range(git_revwalk *walk, const char *range)
{
	git_revwalk__push_options opts = GIT_REVWALK__PUSH_OPTIONS_INIT;
	git_revspec revspec;
	int error = 0;

	if ((error = git_revparse(&revspec, walk->repo, range)))
		return error;

	if (!revspec.to) {
		git_error_set(GIT_ERROR_INVALID, "invalid revspec: range not provided");
		error = GIT_EINVALIDSPEC;
		goto out;
	}

	if (revspec.flags & GIT_REVSPEC_MERGE_BASE) {
		/* TODO: support "<commit>...<commit>" */
		git_error_set(GIT_ERROR_INVALID, "symmetric differences not implemented in revwalk");
		error = GIT_EINVALIDSPEC;
		goto out;
	}

	opts.uninteresting = 1;
	if ((error = git_revwalk__push_commit(walk, git_object_id(revspec.from), &opts)))
		goto out;

	opts.uninteresting = 0;
	error = git_revwalk__push_commit(walk, git_object_id(revspec.to), &opts);

out:
	git_object_free(revspec.from);
	git_object_free(revspec.to);
	return error;
}

int git_revwalk_hide_ref(git_revwalk *walk, const char *refname)
{
	git_revwalk__push_options opts = GIT_REVWALK__PUSH_OPTIONS_INIT;

	GIT_ASSERT_ARG(walk);
	GIT_ASSERT_ARG(refname);

	opts.uninteresting = 1;
	return git_revwalk__push_ref(walk, refname, &opts);
}

static int revwalk_enqueue_timesort(git_revwalk *walk, git_commit_list_node *commit)
{
	return git_pqueue_insert(&walk->iterator_time, commit);
}

static int revwalk_enqueue_unsorted(git_revwalk *walk, git_commit_list_node *commit)
{
	return git_commit_list_insert(commit, &walk->iterator_rand) ? 0 : -1;
}

static int revwalk_next_timesort(git_commit_list_node **object_out, git_revwalk *walk)
{
	git_commit_list_node *next;

	while ((next = git_pqueue_pop(&walk->iterator_time)) != NULL) {
		/* Some commits might become uninteresting after being added to the list */
		if (!next->uninteresting) {
			*object_out = next;
			return 0;
		}
	}

	git_error_clear();
	return GIT_ITEROVER;
}

static int revwalk_next_unsorted(git_commit_list_node **object_out, git_revwalk *walk)
{
	int error;
	git_commit_list_node *next;

	while (!(error = get_revision(&next, walk, &walk->iterator_rand))) {
		/* Some commits might become uninteresting after being added to the list */
		if (!next->uninteresting) {
			*object_out = next;
			return 0;
		}
	}

	return error;
}

static int revwalk_next_toposort(git_commit_list_node **object_out, git_revwalk *walk)
{
	int error;
	git_commit_list_node *next;

	while (!(error = get_revision(&next, walk, &walk->iterator_topo))) {
		/* Some commits might become uninteresting after being added to the list */
		if (!next->uninteresting) {
			*object_out = next;
			return 0;
		}
	}

	return error;
}

static int revwalk_next_reverse(git_commit_list_node **object_out, git_revwalk *walk)
{
	*object_out = git_commit_list_pop(&walk->iterator_reverse);
	return *object_out ? 0 : GIT_ITEROVER;
}

static void mark_parents_uninteresting(git_commit_list_node *commit)
{
	unsigned short i;
	git_commit_list *parents = NULL;

	for (i = 0; i < commit->out_degree; i++)
		git_commit_list_insert(commit->parents[i], &parents);


	while (parents) {
		commit = git_commit_list_pop(&parents);

		while (commit) {
			if (commit->uninteresting)
				break;

			commit->uninteresting = 1;
			/*
			 * If we've reached this commit some other way
			 * already, we need to mark its parents uninteresting
			 * as well.
			 */
			if (!commit->parents)
				break;

			for (i = 0; i < commit->out_degree; i++)
				git_commit_list_insert(commit->parents[i], &parents);
			commit = commit->parents[0];
		}
	}
}

static int add_parents_to_list(git_revwalk *walk, git_commit_list_node *commit, git_commit_list **list)
{
	unsigned short i;
	int error;

	if (commit->added)
		return 0;

	commit->added = 1;

	/*
	 * Go full on in the uninteresting case as we want to include
	 * as many of these as we can.
	 *
	 * Usually we haven't parsed the parent of a parent, but if we
	 * have it we reached it via other means so we want to mark
	 * its parents recursively too.
	 */
	if (commit->uninteresting) {
		for (i = 0; i < commit->out_degree; i++) {
			git_commit_list_node *p = commit->parents[i];
			p->uninteresting = 1;

			/* git does it gently here, but we don't like missing objects */
			if ((error = git_commit_list_parse(walk, p)) < 0)
				return error;

			if (p->parents)
				mark_parents_uninteresting(p);

			p->seen = 1;
			git_commit_list_insert_by_date(p, list);
		}

		return 0;
	}

	/*
	 * Now on to what we do if the commit is indeed
	 * interesting. Here we do want things like first-parent take
	 * effect as this is what we'll be showing.
	 */
	for (i = 0; i < commit->out_degree; i++) {
		git_commit_list_node *p = commit->parents[i];

		if ((error = git_commit_list_parse(walk, p)) < 0)
			return error;

		if (walk->hide_cb && walk->hide_cb(&p->oid, walk->hide_cb_payload))
			continue;

		if (!p->seen) {
			p->seen = 1;
			git_commit_list_insert_by_date(p, list);
		}

		if (walk->first_parent)
			break;
	}
	return 0;
}

/* How many uninteresting commits we want to look at after we run out of interesting ones */
#define SLOP 5

static int still_interesting(git_commit_list *list, int64_t time, int slop)
{
	/* The empty list is pretty boring */
	if (!list)
		return 0;

	/*
	 * If the destination list has commits with an earlier date than our
	 * source, we want to reset the slop counter as we're not done.
	 */
	if (time <= list->item->time)
		return SLOP;

	for (; list; list = list->next) {
		/*
		 * If the destination list still contains interesting commits we
		 * want to continue looking.
		 */
		if (!list->item->uninteresting || list->item->time > time)
			return SLOP;
	}

	/* Everything's uninteresting, reduce the count */
	return slop - 1;
}

static bool include_path_delta(git_revwalk *walk,
	git_tree *commit_tree,
	git_tree *parent_tree,
	git_diff_options *diffopts)
{
	bool include = false;
	git_diff *diff;
	if (git_diff_tree_to_tree(&diff, walk->repo, parent_tree, commit_tree, diffopts) == 0) {
		size_t num_deltas = git_diff_num_deltas(diff);
		size_t i;
		for (i = 0; i < num_deltas && !include; i++) {
			const git_diff_delta *delta = git_diff_get_delta(diff, i);
			if (delta->new_file.path
				&& git_pathspec__match(&walk->pathspec->pathspec,
					delta->new_file.path, false, false, NULL, NULL)) {
				include = true;
			}
			else if (delta->old_file.path
				&& git_pathspec__match(&walk->pathspec->pathspec,
					delta->old_file.path, false, false, NULL, NULL)) {
				include = true;
			}
		}
		git_diff_free(diff);
	}
	return include;
}

static bool include_path_wildcard(git_revwalk *walk, git_commit *commit, git_tree *commit_tree)
{
	unsigned int parents = git_commit_parentcount(commit);
	git_diff_options diffopts = GIT_DIFF_OPTIONS_INIT;
	bool include = false;

	/* We could narrow this down further by copying over the entire pathspec,
	 * but that doesn't seem to make any difference in performance.
	 * So for now, the just the prefix */
	if (walk->pathspec->prefix) {
		diffopts.pathspec.strings = &walk->pathspec->prefix;
		diffopts.pathspec.count = 1;
	}

	if (parents == 0) {
		include = include_path_delta(walk, commit_tree, NULL, &diffopts);
	}
	else {
		unsigned int i;
		include = true;
		/* Loop through all parents, and ensure that it matches with all 
		 *  parents before including the commit
		 */
		for (i = 0; i < parents && include; i++) {
			git_commit *parent = NULL;
			git_tree *parent_tree = NULL;
			/*Assume it's to be excluded unless the delta matches*/
			include = false;
			if (git_commit_parent(&parent, commit, i) == 0
				&& git_commit_tree(&parent_tree, parent) == 0) {
				if (include_path_delta(walk, commit_tree, parent_tree, &diffopts))
					include = true;
			}
			git_tree_free(parent_tree);
			git_commit_free(parent);
		}
	}
	return include;
}

static bool include_path_exact_root(git_revwalk *walk, git_tree *commit_tree)
{
	size_t i;
	git_attr_fnmatch *match;
	/* If it's a root commit, we just need to find the first path that matches */
	git_vector_foreach(&walk->pathspec->pathspec, i, match) {
		git_tree_entry *entry=NULL;
		if (git_tree_entry_bypath(&entry, commit_tree, match->pattern) == 0) {
			git_tree_entry_free(entry);
			return true;
		}
	}
	return false;
}

static bool include_path_exact_parent(git_revwalk *walk, git_tree *commit_tree, git_tree *parent_tree)
{
	size_t i;
	git_attr_fnmatch *match;
	bool include = false;
	git_vector_foreach(&walk->pathspec->pathspec, i, match) {
		git_tree_entry *commit_entry=NULL;
		git_tree_entry *parent_entry=NULL;

		/* Given we are working with full paths here, we only need to look at OIDs
		 * to know if the path is touched by the commit */
		git_tree_entry_bypath(&commit_entry, commit_tree, match->pattern);
		git_tree_entry_bypath(&parent_entry, parent_tree, match->pattern);

		/* If the existance of an entry is different, it means we deal with
		 * an add or remove case. We don't need to think about renaming here
		 * since that would still count as a change */
		if ((commit_entry == NULL) != (parent_entry == NULL)) {
			include = true;
		}
		/* Both trees have an entry. Include if the OID is different between the trees */
		else if (commit_entry && parent_entry
			&& git_oid_equal(
				git_tree_entry_id(commit_entry),
				git_tree_entry_id(parent_entry)) == 0) {
			include = true;
		}
		git_tree_entry_free(commit_entry);
		git_tree_entry_free(parent_entry);
		if (include)
			return include;
	}
	return false;
}

static bool include_path_exact(git_revwalk *walk, git_commit *commit, git_tree *commit_tree) {
	unsigned int parents = git_commit_parentcount(commit);
	if (parents == 0) {
		return include_path_exact_root(walk, commit_tree);
	}
	else {
		unsigned int p;
		bool include_commit = true;
		/* Loop through all parents, and ensure that it matches with all 
		 *  parents before including the commit
		 */
		for (p = 0; p < parents && include_commit; p++) {
			git_commit *parent = NULL;
			git_tree *parent_tree = NULL;
			if (git_commit_parent(&parent, commit, p) == 0
				&& git_commit_tree(&parent_tree, parent) == 0) {
				if (!include_path_exact_parent(walk, commit_tree, parent_tree))
					include_commit = false;
			}
			git_tree_free(parent_tree);
			git_commit_free(parent);
		}
		return include_commit;
	}
}

static bool include_path(git_revwalk *walk, git_commit_list_node *commit_node)
{
	git_commit *commit = NULL;
	git_tree *commit_tree = NULL;
	bool include = false;

	if (git_commit_lookup(&commit, walk->repo, &commit_node->oid) == 0
		&& git_commit_tree(&commit_tree, commit) == 0) {
		if (walk->pathspec_wildcard)
			include = include_path_wildcard(walk, commit, commit_tree);
		else
			include = include_path_exact(walk, commit, commit_tree);
	}

	git_tree_free(commit_tree);
	git_commit_free(commit);
	return include;
}

static int limit_list(git_commit_list **out, git_revwalk *walk, git_commit_list *commits)
{
	int error, slop = SLOP;
	int64_t time = INT64_MAX;
	git_commit_list *list = commits;
	git_commit_list *newlist = NULL;
	git_commit_list **p = &newlist;

	while (list) {
		git_commit_list_node *commit = git_commit_list_pop(&list);

		if ((error = add_parents_to_list(walk, commit, &list)) < 0)
			return error;

		if (commit->uninteresting) {
			mark_parents_uninteresting(commit);

			slop = still_interesting(list, time, slop);
			if (slop)
				continue;

			break;
		}

		if (walk->pathspec && !include_path(walk, commit))
			continue;

		if (walk->hide_cb && walk->hide_cb(&commit->oid, walk->hide_cb_payload))
			continue;

		time = commit->time;
		p = &git_commit_list_insert(commit, p)->next;
	}

	git_commit_list_free(&list);
	*out = newlist;
	return 0;
}

static int get_revision(git_commit_list_node **out, git_revwalk *walk, git_commit_list **list)
{
	int error;
	git_commit_list_node *commit;

	commit = git_commit_list_pop(list);
	if (!commit) {
		git_error_clear();
		return GIT_ITEROVER;
	}

	/*
	 * If we did not run limit_list and we must add parents to the
	 * list ourselves.
	 */
	if (!walk->limited) {
		if ((error = add_parents_to_list(walk, commit, list)) < 0)
			return error;
	}

	*out = commit;
	return 0;
}

static int sort_in_topological_order(git_commit_list **out, git_revwalk *walk, git_commit_list *list)
{
	git_commit_list *ll = NULL, *newlist, **pptr;
	git_commit_list_node *next;
	git_pqueue queue;
	git_vector_cmp queue_cmp = NULL;
	unsigned short i;
	int error;

	if (walk->sorting & GIT_SORT_TIME)
		queue_cmp = git_commit_list_time_cmp;

	if ((error = git_pqueue_init(&queue, 0, 8, queue_cmp)))
		return error;

	/*
	 * Start by resetting the in-degree to 1 for the commits in
	 * our list. We want to go through this list again, so we
	 * store it in the commit list as we extract it from the lower
	 * machinery.
	 */
	for (ll = list; ll; ll = ll->next) {
		ll->item->in_degree = 1;
	}

	/*
	 * Count up how many children each commit has. We limit
	 * ourselves to those commits in the original list (in-degree
	 * of 1) avoiding setting it for any parent that was hidden.
	 */
	for(ll = list; ll; ll = ll->next) {
		for (i = 0; i < ll->item->out_degree; ++i) {
			git_commit_list_node *parent = ll->item->parents[i];
			if (parent->in_degree)
				parent->in_degree++;
		}
	}

	/*
	 * Now we find the tips i.e. those not reachable from any other node
	 * i.e. those which still have an in-degree of 1.
	 */
	for(ll = list; ll; ll = ll->next) {
		if (ll->item->in_degree == 1) {
			if ((error = git_pqueue_insert(&queue, ll->item)))
				goto cleanup;
		}
	}

	/*
	 * We need to output the tips in the order that they came out of the
	 * traversal, so if we're not doing time-sorting, we need to reverse the
	 * pqueue in order to get them to come out as we inserted them.
	 */
	if ((walk->sorting & GIT_SORT_TIME) == 0)
		git_pqueue_reverse(&queue);


	pptr = &newlist;
	newlist = NULL;
	while ((next = git_pqueue_pop(&queue)) != NULL) {
		for (i = 0; i < next->out_degree; ++i) {
			git_commit_list_node *parent = next->parents[i];
			if (parent->in_degree == 0)
				continue;

			if (--parent->in_degree == 1) {
				if ((error = git_pqueue_insert(&queue, parent)))
					goto cleanup;
			}
		}

		/* All the children of 'item' have been emitted (since we got to it via the priority queue) */
		next->in_degree = 0;

		pptr = &git_commit_list_insert(next, pptr)->next;
	}

	*out = newlist;
	error = 0;

cleanup:
	git_pqueue_free(&queue);
	return error;
}

static int prepare_walk(git_revwalk *walk)
{
	int error = 0;
	git_commit_list *list, *commits = NULL, *commits_last = NULL;
	git_commit_list_node *next;

	/* If there were no pushes, we know that the walk is already over */
	if (!walk->did_push) {
		git_error_clear();
		return GIT_ITEROVER;
	}

	/*
	 * This is a bit convoluted, but necessary to maintain the order of
	 * the commits. This is especially important in situations where
	 * git_revwalk__push_glob is called with a git_revwalk__push_options
	 * setting insert_by_date = 1, which is critical for fetch negotiation.
	 */
	for (list = walk->user_input; list; list = list->next) {
		git_commit_list_node *commit = list->item;
		if ((error = git_commit_list_parse(walk, commit)) < 0)
			return error;

		if (commit->uninteresting)
			mark_parents_uninteresting(commit);

		if (!commit->seen) {
			git_commit_list *new_list = NULL;
			if ((new_list = git_commit_list_create(commit, NULL)) == NULL) {
				git_error_set_oom();
				return -1;
			}

			commit->seen = 1;
			if (commits_last == NULL)
				commits = new_list;
			else
				commits_last->next = new_list;

			commits_last = new_list;
		}
	}

	if (walk->limited && (error = limit_list(&commits, walk, commits)) < 0)
		return error;

	if (walk->sorting & GIT_SORT_TOPOLOGICAL) {
		error = sort_in_topological_order(&walk->iterator_topo, walk, commits);
		git_commit_list_free(&commits);

		if (error < 0)
			return error;

		walk->get_next = &revwalk_next_toposort;
	} else if (walk->sorting & GIT_SORT_TIME) {
		for (list = commits; list && !error; list = list->next)
			error = walk->enqueue(walk, list->item);

		git_commit_list_free(&commits);

		if (error < 0)
			return error;
	} else {
		walk->iterator_rand = commits;
		walk->get_next = revwalk_next_unsorted;
	}

	if (walk->sorting & GIT_SORT_REVERSE) {

		while ((error = walk->get_next(&next, walk)) == 0)
			if (git_commit_list_insert(next, &walk->iterator_reverse) == NULL)
				return -1;

		if (error != GIT_ITEROVER)
			return error;

		walk->get_next = &revwalk_next_reverse;
	}

	walk->walking = 1;
	return 0;
}


int git_revwalk_new(git_revwalk **revwalk_out, git_repository *repo)
{
	git_revwalk *walk = git__calloc(1, sizeof(git_revwalk));
	GIT_ERROR_CHECK_ALLOC(walk);

	if (git_pqueue_init(&walk->iterator_time, 0, 8, git_commit_list_time_cmp) < 0 ||
	    git_pool_init(&walk->commit_pool, COMMIT_ALLOC) < 0)
		return -1;

	walk->get_next = &revwalk_next_unsorted;
	walk->enqueue = &revwalk_enqueue_unsorted;

	walk->repo = repo;

	if (git_repository_odb(&walk->odb, repo) < 0) {
		git_revwalk_free(walk);
		return -1;
	}

	*revwalk_out = walk;
	return 0;
}

void git_revwalk_free(git_revwalk *walk)
{
	if (walk == NULL)
		return;

	git_revwalk_reset(walk);
	git_odb_free(walk->odb);

	git_revwalk_oidmap_dispose(&walk->commits);
	git_pool_clear(&walk->commit_pool);
	git_pqueue_free(&walk->iterator_time);
	git__free(walk);
}

git_repository *git_revwalk_repository(git_revwalk *walk)
{
	GIT_ASSERT_ARG_WITH_RETVAL(walk, NULL);

	return walk->repo;
}

int git_revwalk_sorting(git_revwalk *walk, unsigned int sort_mode)
{
	GIT_ASSERT_ARG(walk);

	if (walk->walking)
		git_revwalk_reset(walk);

	walk->sorting = sort_mode;

	if (walk->sorting & GIT_SORT_TIME) {
		walk->get_next = &revwalk_next_timesort;
		walk->enqueue = &revwalk_enqueue_timesort;
	} else {
		walk->get_next = &revwalk_next_unsorted;
		walk->enqueue = &revwalk_enqueue_unsorted;
	}

	if (walk->sorting != GIT_SORT_NONE)
		walk->limited = 1;

	return 0;
}

int git_revwalk_simplify_first_parent(git_revwalk *walk)
{
	walk->first_parent = 1;
	return 0;
}

int git_revwalk_next(git_oid *oid, git_revwalk *walk)
{
	int error;
	git_commit_list_node *next;

	GIT_ASSERT_ARG(walk);
	GIT_ASSERT_ARG(oid);

	if (!walk->walking) {
		if ((error = prepare_walk(walk)) < 0)
			return error;
	}

	error = walk->get_next(&next, walk);

	if (error == GIT_ITEROVER) {
		git_revwalk_reset(walk);
		git_error_clear();
		return GIT_ITEROVER;
	}

	if (!error)
		git_oid_cpy(oid, &next->oid);

	return error;
}

static bool pathspec_has_wildcard(git_pathspec *pathspec)
{
	size_t i;
	git_attr_fnmatch *match;
	git_vector_foreach(&pathspec->pathspec, i, match) {
		if (match->flags & GIT_ATTR_FNMATCH_HASWILD) {
			return true;
		}
	}
	return false;
}

int git_revwalk_pathspec(git_revwalk *walk, git_pathspec *pathspec) {
	GIT_ASSERT_ARG(walk);

	if (walk->walking)
		git_revwalk_reset(walk);

	if (pathspec) {
		walk->pathspec = pathspec;
		walk->pathspec_wildcard = pathspec_has_wildcard(pathspec);
		walk->limited = 1;
	}

	return 0;
}

int git_revwalk_reset(git_revwalk *walk)
{
	git_commit_list_node *commit;
	git_hashmap_iter_t iter = GIT_HASHMAP_ITER_INIT;

	GIT_ASSERT_ARG(walk);

	while (git_revwalk_oidmap_iterate(&iter, NULL, &commit, &walk->commits) == 0) {
		commit->seen = 0;
		commit->in_degree = 0;
		commit->topo_delay = 0;
		commit->uninteresting = 0;
		commit->added = 0;
		commit->flags = 0;
	}

	git_pqueue_clear(&walk->iterator_time);
	git_commit_list_free(&walk->iterator_topo);
	git_commit_list_free(&walk->iterator_rand);
	git_commit_list_free(&walk->iterator_reverse);
	git_commit_list_free(&walk->user_input);
	walk->first_parent = 0;
	walk->walking = 0;
	walk->limited = 0;
	walk->did_push = walk->did_hide = 0;
	walk->sorting = GIT_SORT_NONE;

	return 0;
}

int git_revwalk_add_hide_cb(
	git_revwalk *walk,
	git_revwalk_hide_cb hide_cb,
	void *payload)
{
	GIT_ASSERT_ARG(walk);

	if (walk->walking)
		git_revwalk_reset(walk);

	walk->hide_cb = hide_cb;
	walk->hide_cb_payload = payload;

	if (hide_cb)
		walk->limited = 1;

	return 0;
}

