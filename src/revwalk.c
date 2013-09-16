/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "commit.h"
#include "odb.h"
#include "pool.h"

#include "revwalk.h"
#include "git2/revparse.h"
#include "merge.h"

#include <regex.h>

git_commit_list_node *git_revwalk__commit_lookup(
	git_revwalk *walk, const git_oid *oid)
{
	git_commit_list_node *commit;
	khiter_t pos;
	int ret;

	/* lookup and reserve space if not already present */
	pos = kh_get(oid, walk->commits, oid);
	if (pos != kh_end(walk->commits))
		return kh_value(walk->commits, pos);

	commit = git_commit_list_alloc_node(walk);
	if (commit == NULL)
		return NULL;

	git_oid_cpy(&commit->oid, oid);

	pos = kh_put(oid, walk->commits, &commit->oid, &ret);
	assert(ret != 0);
	kh_value(walk->commits, pos) = commit;

	return commit;
}

static int mark_uninteresting(git_commit_list_node *commit)
{
	unsigned short i;
	git_array_t(git_commit_list_node *) pending = GIT_ARRAY_INIT;
	git_commit_list_node **tmp;

	assert(commit);

	git_array_alloc(pending);
	GITERR_CHECK_ARRAY(pending);

	do {
		commit->uninteresting = 1;

		/* This means we've reached a merge base, so there's no need to walk any more */
		if ((commit->flags & (RESULT | STALE)) == RESULT) {
			tmp = git_array_pop(pending);
			commit = tmp ? *tmp : NULL;
			continue;
		}

		for (i = 0; i < commit->out_degree; ++i)
			if (!commit->parents[i]->uninteresting) {
				git_commit_list_node **node = git_array_alloc(pending);
				GITERR_CHECK_ALLOC(node);
				*node = commit->parents[i];
			}

		tmp = git_array_pop(pending);
		commit = tmp ? *tmp : NULL;

	} while (git_array_size(pending) > 0);

	git_array_clear(pending);

	return 0;
}

static int process_commit(git_revwalk *walk, git_commit_list_node *commit, int hide)
{
	int error;

	if (hide && mark_uninteresting(commit) < 0)
		return -1;

	if (commit->seen)
		return 0;

	commit->seen = 1;

	if ((error = git_commit_list_parse(walk, commit)) < 0)
		return error;

	return walk->enqueue(walk, commit);
}

static int process_commit_parents(git_revwalk *walk, git_commit_list_node *commit)
{
	unsigned short i, max;
	int error = 0;

	max = commit->out_degree;
	if (walk->first_parent && commit->out_degree)
		max = 1;

	for (i = 0; i < max && !error; ++i)
		error = process_commit(walk, commit->parents[i], commit->uninteresting);

	return error;
}

static int push_commit(git_revwalk *walk, const git_oid *oid, int uninteresting)
{
	git_object *obj;
	git_otype type;
	git_commit_list_node *commit;

	if (git_object_lookup(&obj, walk->repo, oid, GIT_OBJ_ANY) < 0)
		return -1;

	type = git_object_type(obj);
	git_object_free(obj);

	if (type != GIT_OBJ_COMMIT) {
		giterr_set(GITERR_INVALID, "Object is no commit object");
		return -1;
	}

	commit = git_revwalk__commit_lookup(walk, oid);
	if (commit == NULL)
		return -1; /* error already reported by failed lookup */

	commit->uninteresting = uninteresting;
	if (walk->one == NULL && !uninteresting) {
		walk->one = commit;
	} else {
		if (git_vector_insert(&walk->twos, commit) < 0)
			return -1;
	}

	return 0;
}

int git_revwalk_push(git_revwalk *walk, const git_oid *oid)
{
	assert(walk && oid);
	return push_commit(walk, oid, 0);
}


int git_revwalk_hide(git_revwalk *walk, const git_oid *oid)
{
	assert(walk && oid);
	return push_commit(walk, oid, 1);
}

static int push_ref(git_revwalk *walk, const char *refname, int hide)
{
	git_oid oid;

	if (git_reference_name_to_id(&oid, walk->repo, refname) < 0)
		return -1;

	return push_commit(walk, &oid, hide);
}

struct push_cb_data {
	git_revwalk *walk;
	int hide;
};

static int push_glob_cb(const char *refname, void *data_)
{
	struct push_cb_data *data = (struct push_cb_data *)data_;

	return push_ref(data->walk, refname, data->hide);
}

static int push_glob(git_revwalk *walk, const char *glob, int hide)
{
	git_buf buf = GIT_BUF_INIT;
	struct push_cb_data data;
	regex_t preg;

	assert(walk && glob);

	/* refs/ is implied if not given in the glob */
	if (strncmp(glob, GIT_REFS_DIR, strlen(GIT_REFS_DIR))) {
		git_buf_printf(&buf, GIT_REFS_DIR "%s", glob);
	} else {
		git_buf_puts(&buf, glob);
	}

	/* If no '?', '*' or '[' exist, we append '/ *' to the glob */
	memset(&preg, 0x0, sizeof(regex_t));
	if (regcomp(&preg, "[?*[]", REG_EXTENDED)) {
		giterr_set(GITERR_OS, "Regex failed to compile");
		git_buf_free(&buf);
		return -1;
	}

	if (regexec(&preg, glob, 0, NULL, 0))
		git_buf_puts(&buf, "/*");

	if (git_buf_oom(&buf))
		goto on_error;

	data.walk = walk;
	data.hide = hide;

	if (git_reference_foreach_glob(
		walk->repo, git_buf_cstr(&buf), push_glob_cb, &data) < 0)
		goto on_error;

	regfree(&preg);
	git_buf_free(&buf);
	return 0;

on_error:
	regfree(&preg);
	git_buf_free(&buf);
	return -1;
}

int git_revwalk_push_glob(git_revwalk *walk, const char *glob)
{
	assert(walk && glob);
	return push_glob(walk, glob, 0);
}

int git_revwalk_hide_glob(git_revwalk *walk, const char *glob)
{
	assert(walk && glob);
	return push_glob(walk, glob, 1);
}

int git_revwalk_push_head(git_revwalk *walk)
{
	assert(walk);
	return push_ref(walk, GIT_HEAD_FILE, 0);
}

int git_revwalk_hide_head(git_revwalk *walk)
{
	assert(walk);
	return push_ref(walk, GIT_HEAD_FILE, 1);
}

int git_revwalk_push_ref(git_revwalk *walk, const char *refname)
{
	assert(walk && refname);
	return push_ref(walk, refname, 0);
}

int git_revwalk_push_range(git_revwalk *walk, const char *range)
{
	git_revspec revspec;
	int error = 0;

	if ((error = git_revparse(&revspec, walk->repo, range)))
		return error;

	if (revspec.flags & GIT_REVPARSE_MERGE_BASE) {
		/* TODO: support "<commit>...<commit>" */
		giterr_set(GITERR_INVALID, "Symmetric differences not implemented in revwalk");
		return GIT_EINVALIDSPEC;
	}

	if ((error = push_commit(walk, git_object_id(revspec.from), 1)))
		goto out;

	error = push_commit(walk, git_object_id(revspec.to), 0);

out:
	git_object_free(revspec.from);
	git_object_free(revspec.to);
	return error;
}

int git_revwalk_hide_ref(git_revwalk *walk, const char *refname)
{
	assert(walk && refname);
	return push_ref(walk, refname, 1);
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
	int error;
	git_commit_list_node *next;

	while ((next = git_pqueue_pop(&walk->iterator_time)) != NULL) {
		if ((error = process_commit_parents(walk, next)) < 0)
			return error;

		if (!next->uninteresting) {
			*object_out = next;
			return 0;
		}
	}

	giterr_clear();
	return GIT_ITEROVER;
}

static int revwalk_next_unsorted(git_commit_list_node **object_out, git_revwalk *walk)
{
	int error;
	git_commit_list_node *next;

	while ((next = git_commit_list_pop(&walk->iterator_rand)) != NULL) {
		if ((error = process_commit_parents(walk, next)) < 0)
			return error;

		if (!next->uninteresting) {
			*object_out = next;
			return 0;
		}
	}

	giterr_clear();
	return GIT_ITEROVER;
}

static int revwalk_next_toposort(git_commit_list_node **object_out, git_revwalk *walk)
{
	git_commit_list_node *next;
	unsigned short i, max;

	for (;;) {
		next = git_commit_list_pop(&walk->iterator_topo);
		if (next == NULL) {
			giterr_clear();
			return GIT_ITEROVER;
		}

		if (next->in_degree > 0) {
			next->topo_delay = 1;
			continue;
		}


		max = next->out_degree;
		if (walk->first_parent && next->out_degree)
			max = 1;

		for (i = 0; i < max; ++i) {
			git_commit_list_node *parent = next->parents[i];

			if (--parent->in_degree == 0 && parent->topo_delay) {
				parent->topo_delay = 0;
				if (git_commit_list_insert(parent, &walk->iterator_topo) == NULL)
					return -1;
			}
		}

		*object_out = next;
		return 0;
	}
}

static int revwalk_next_reverse(git_commit_list_node **object_out, git_revwalk *walk)
{
	*object_out = git_commit_list_pop(&walk->iterator_reverse);
	return *object_out ? 0 : GIT_ITEROVER;
}


static int prepare_walk(git_revwalk *walk)
{
	int error;
	unsigned int i;
	git_commit_list_node *next, *two;
	git_commit_list *bases = NULL;

	/*
	 * If walk->one is NULL, there were no positive references,
	 * so we know that the walk is already over.
	 */
	if (walk->one == NULL) {
		giterr_clear();
		return GIT_ITEROVER;
	}

	/* first figure out what the merge bases are */
	if (git_merge__bases_many(&bases, walk, walk->one, &walk->twos) < 0)
		return -1;

	git_commit_list_free(&bases);
	if (process_commit(walk, walk->one, walk->one->uninteresting) < 0)
		return -1;

	git_vector_foreach(&walk->twos, i, two) {
		if (process_commit(walk, two, two->uninteresting) < 0)
			return -1;
	}

	if (walk->sorting & GIT_SORT_TOPOLOGICAL) {
		unsigned short i;

		while ((error = walk->get_next(&next, walk)) == 0) {
			for (i = 0; i < next->out_degree; ++i) {
				git_commit_list_node *parent = next->parents[i];
				parent->in_degree++;
			}

			if (git_commit_list_insert(next, &walk->iterator_topo) == NULL)
				return -1;
		}

		if (error != GIT_ITEROVER)
			return error;

		walk->get_next = &revwalk_next_toposort;
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
	git_revwalk *walk;

	walk = git__malloc(sizeof(git_revwalk));
	GITERR_CHECK_ALLOC(walk);

	memset(walk, 0x0, sizeof(git_revwalk));

	walk->commits = git_oidmap_alloc();
	GITERR_CHECK_ALLOC(walk->commits);

	if (git_pqueue_init(&walk->iterator_time, 8, git_commit_list_time_cmp) < 0 ||
		git_vector_init(&walk->twos, 4, NULL) < 0 ||
		git_pool_init(&walk->commit_pool, 1,
			git_pool__suggest_items_per_page(COMMIT_ALLOC) * COMMIT_ALLOC) < 0)
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

	git_oidmap_free(walk->commits);
	git_pool_clear(&walk->commit_pool);
	git_pqueue_free(&walk->iterator_time);
	git_vector_free(&walk->twos);
	git__free(walk);
}

git_repository *git_revwalk_repository(git_revwalk *walk)
{
	assert(walk);
	return walk->repo;
}

void git_revwalk_sorting(git_revwalk *walk, unsigned int sort_mode)
{
	assert(walk);

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
}

void git_revwalk_simplify_first_parent(git_revwalk *walk)
{
	walk->first_parent = 1;
}

int git_revwalk_next(git_oid *oid, git_revwalk *walk)
{
	int error;
	git_commit_list_node *next;

	assert(walk && oid);

	if (!walk->walking) {
		if ((error = prepare_walk(walk)) < 0)
			return error;
	}

	error = walk->get_next(&next, walk);

	if (error == GIT_ITEROVER) {
		git_revwalk_reset(walk);
		giterr_clear();
		return GIT_ITEROVER;
	}

	if (!error)
		git_oid_cpy(oid, &next->oid);

	return error;
}

void git_revwalk_reset(git_revwalk *walk)
{
	git_commit_list_node *commit;

	assert(walk);

	kh_foreach_value(walk->commits, commit, {
		commit->seen = 0;
		commit->in_degree = 0;
		commit->topo_delay = 0;
		commit->uninteresting = 0;
		});

	git_pqueue_clear(&walk->iterator_time);
	git_commit_list_free(&walk->iterator_topo);
	git_commit_list_free(&walk->iterator_rand);
	git_commit_list_free(&walk->iterator_reverse);
	walk->walking = 0;

	walk->one = NULL;
	git_vector_clear(&walk->twos);
}

