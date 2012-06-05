/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "commit.h"
#include "odb.h"
#include "pqueue.h"
#include "pool.h"
#include "oidmap.h"

#include "git2/revwalk.h"
#include "git2/merge.h"

#include <regex.h>

GIT__USE_OIDMAP;

#define PARENT1  (1 << 0)
#define PARENT2  (1 << 1)
#define RESULT   (1 << 2)
#define STALE    (1 << 3)

typedef struct commit_object {
	git_oid oid;
	uint32_t time;
	unsigned int seen:1,
			 uninteresting:1,
			 topo_delay:1,
			 parsed:1,
			 flags : 4;

	unsigned short in_degree;
	unsigned short out_degree;

	struct commit_object **parents;
} commit_object;

typedef struct commit_list {
	commit_object *item;
	struct commit_list *next;
} commit_list;

struct git_revwalk {
	git_repository *repo;
	git_odb *odb;

	git_oidmap *commits;
	git_pool commit_pool;

	commit_list *iterator_topo;
	commit_list *iterator_rand;
	commit_list *iterator_reverse;
	git_pqueue iterator_time;

	int (*get_next)(commit_object **, git_revwalk *);
	int (*enqueue)(git_revwalk *, commit_object *);

	unsigned walking:1;
	unsigned int sorting;

	/* merge base calculation */
	commit_object *one;
	git_vector twos;
};

static int commit_time_cmp(void *a, void *b)
{
	commit_object *commit_a = (commit_object *)a;
	commit_object *commit_b = (commit_object *)b;

	return (commit_a->time < commit_b->time);
}

static commit_list *commit_list_insert(commit_object *item, commit_list **list_p)
{
	commit_list *new_list = git__malloc(sizeof(commit_list));
	if (new_list != NULL) {
		new_list->item = item;
		new_list->next = *list_p;
	}
	*list_p = new_list;
	return new_list;
}

static commit_list *commit_list_insert_by_date(commit_object *item, commit_list **list_p)
{
	commit_list **pp = list_p;
	commit_list *p;

	while ((p = *pp) != NULL) {
		if (commit_time_cmp(p->item, item) < 0)
			break;

		pp = &p->next;
	}

	return commit_list_insert(item, pp);
}
static void commit_list_free(commit_list **list_p)
{
	commit_list *list = *list_p;

	while (list) {
		commit_list *temp = list;
		list = temp->next;
		git__free(temp);
	}

	*list_p = NULL;
}

static commit_object *commit_list_pop(commit_list **stack)
{
	commit_list *top = *stack;
	commit_object *item = top ? top->item : NULL;

	if (top) {
		*stack = top->next;
		git__free(top);
	}
	return item;
}

#define PARENTS_PER_COMMIT	2
#define COMMIT_ALLOC \
	(sizeof(commit_object) + PARENTS_PER_COMMIT * sizeof(commit_object *))

static commit_object *alloc_commit(git_revwalk *walk)
{
	return (commit_object *)git_pool_malloc(&walk->commit_pool, COMMIT_ALLOC);
}

static commit_object **alloc_parents(
	git_revwalk *walk, commit_object *commit, size_t n_parents)
{
	if (n_parents <= PARENTS_PER_COMMIT)
		return (commit_object **)((char *)commit + sizeof(commit_object));

	return (commit_object **)git_pool_malloc(
		&walk->commit_pool, (uint32_t)(n_parents * sizeof(commit_object *)));
}


static commit_object *commit_lookup(git_revwalk *walk, const git_oid *oid)
{
	commit_object *commit;
	khiter_t pos;
	int ret;

	/* lookup and reserve space if not already present */
	pos = kh_get(oid, walk->commits, oid);
	if (pos != kh_end(walk->commits))
		return kh_value(walk->commits, pos);

	commit = alloc_commit(walk);
	if (commit == NULL)
		return NULL;

	git_oid_cpy(&commit->oid, oid);

	pos = kh_put(oid, walk->commits, &commit->oid, &ret);
	assert(ret != 0);
	kh_value(walk->commits, pos) = commit;

	return commit;
}

static int commit_quick_parse(git_revwalk *walk, commit_object *commit, git_rawobj *raw)
{
	const size_t parent_len = strlen("parent ") + GIT_OID_HEXSZ + 1;

	unsigned char *buffer = raw->data;
	unsigned char *buffer_end = buffer + raw->len;
	unsigned char *parents_start;

	int i, parents = 0;
	int commit_time;

	buffer += strlen("tree ") + GIT_OID_HEXSZ + 1;

	parents_start = buffer;
	while (buffer + parent_len < buffer_end && memcmp(buffer, "parent ", strlen("parent ")) == 0) {
		parents++;
		buffer += parent_len;
	}

	commit->parents = alloc_parents(walk, commit, parents);
	GITERR_CHECK_ALLOC(commit->parents);

	buffer = parents_start;
	for (i = 0; i < parents; ++i) {
		git_oid oid;

		if (git_oid_fromstr(&oid, (char *)buffer + strlen("parent ")) < 0)
			return -1;

		commit->parents[i] = commit_lookup(walk, &oid);
		if (commit->parents[i] == NULL)
			return -1;

		buffer += parent_len;
	}

	commit->out_degree = (unsigned short)parents;

	if ((buffer = memchr(buffer, '\n', buffer_end - buffer)) == NULL) {
		giterr_set(GITERR_ODB, "Failed to parse commit. Object is corrupted");
		return -1;
	}

	buffer = memchr(buffer, '>', buffer_end - buffer);
	if (buffer == NULL) {
		giterr_set(GITERR_ODB, "Failed to parse commit. Can't find author");
		return -1;
	}

	if (git__strtol32(&commit_time, (char *)buffer + 2, NULL, 10) < 0) {
		giterr_set(GITERR_ODB, "Failed to parse commit. Can't parse commit time");
		return -1;
	}

	commit->time = (time_t)commit_time;
	commit->parsed = 1;
	return 0;
}

static int commit_parse(git_revwalk *walk, commit_object *commit)
{
	git_odb_object *obj;
	int error;

	if (commit->parsed)
		return 0;

	if ((error = git_odb_read(&obj, walk->odb, &commit->oid)) < 0)
		return error;

	if (obj->raw.type != GIT_OBJ_COMMIT) {
		git_odb_object_free(obj);
		giterr_set(GITERR_INVALID, "Failed to parse commit. Object is no commit object");
		return -1;
	}

	error = commit_quick_parse(walk, commit, &obj->raw);
	git_odb_object_free(obj);
	return error;
}

static int interesting(git_pqueue *list)
{
	unsigned int i;
	for (i = 1; i < git_pqueue_size(list); i++) {
		commit_object *commit = list->d[i];
		if ((commit->flags & STALE) == 0)
			return 1;
	}

	return 0;
}

static int merge_bases_many(commit_list **out, git_revwalk *walk, commit_object *one, git_vector *twos)
{
	int error;
	unsigned int i;
	commit_object *two;
	commit_list *result = NULL, *tmp = NULL;
	git_pqueue list;

	/* if the commit is repeated, we have a our merge base already */
	git_vector_foreach(twos, i, two) {
		if (one == two)
			return commit_list_insert(one, out) ? 0 : -1;
	}

	if (git_pqueue_init(&list, twos->length * 2, commit_time_cmp) < 0)
		return -1;

	if (commit_parse(walk, one) < 0)
	    return -1;

	one->flags |= PARENT1;
	if (git_pqueue_insert(&list, one) < 0)
		return -1;

	git_vector_foreach(twos, i, two) {
		commit_parse(walk, two);
		two->flags |= PARENT2;
		if (git_pqueue_insert(&list, two) < 0)
			return -1;
	}

	/* as long as there are non-STALE commits */
	while (interesting(&list)) {
		commit_object *commit;
		int flags;

		commit = git_pqueue_pop(&list);

		flags = commit->flags & (PARENT1 | PARENT2 | STALE);
		if (flags == (PARENT1 | PARENT2)) {
			if (!(commit->flags & RESULT)) {
				commit->flags |= RESULT;
				if (commit_list_insert(commit, &result) == NULL)
					return -1;
			}
			/* we mark the parents of a merge stale */
			flags |= STALE;
		}

		for (i = 0; i < commit->out_degree; i++) {
			commit_object *p = commit->parents[i];
			if ((p->flags & flags) == flags)
				continue;

			if ((error = commit_parse(walk, p)) < 0)
				return error;

			p->flags |= flags;
			if (git_pqueue_insert(&list, p) < 0)
				return -1;
		}
	}

	git_pqueue_free(&list);

	/* filter out any stale commits in the results */
	tmp = result;
	result = NULL;

	while (tmp) {
		struct commit_list *next = tmp->next;
		if (!(tmp->item->flags & STALE))
			if (commit_list_insert_by_date(tmp->item, &result) == NULL)
				return -1;

		git__free(tmp);
		tmp = next;
	}

	*out = result;
	return 0;
}

int git_merge_base(git_oid *out, git_repository *repo, git_oid *one, git_oid *two)
{
	git_revwalk *walk;
	git_vector list;
	commit_list *result = NULL;
	commit_object *commit;
	void *contents[1];

	if (git_revwalk_new(&walk, repo) < 0)
		return -1;

	commit = commit_lookup(walk, two);
	if (commit == NULL)
		goto on_error;

	/* This is just one value, so we can do it on the stack */
	memset(&list, 0x0, sizeof(git_vector));
	contents[0] = commit;
	list.length = 1;
	list.contents = contents;

	commit = commit_lookup(walk, one);
	if (commit == NULL)
		goto on_error;

	if (merge_bases_many(&result, walk, commit, &list) < 0)
		goto on_error;

	if (!result) {
		git_revwalk_free(walk);
		return GIT_ENOTFOUND;
	}

	git_oid_cpy(out, &result->item->oid);
	commit_list_free(&result);
	git_revwalk_free(walk);

	return 0;

on_error:
	git_revwalk_free(walk);
	return -1;
}

static void mark_uninteresting(commit_object *commit)
{
	unsigned short i;
	assert(commit);

	commit->uninteresting = 1;

	/* This means we've reached a merge base, so there's no need to walk any more */
	if ((commit->flags & (RESULT | STALE)) == RESULT)
		return;

	for (i = 0; i < commit->out_degree; ++i)
		if (!commit->parents[i]->uninteresting)
			mark_uninteresting(commit->parents[i]);
}

static int process_commit(git_revwalk *walk, commit_object *commit, int hide)
{
	int error;

	if (hide)
		mark_uninteresting(commit);

	if (commit->seen)
		return 0;

	commit->seen = 1;

	if ((error = commit_parse(walk, commit)) < 0)
		return error;

	return walk->enqueue(walk, commit);
}

static int process_commit_parents(git_revwalk *walk, commit_object *commit)
{
	unsigned short i;
	int error = 0;

	for (i = 0; i < commit->out_degree && !error; ++i)
		error = process_commit(walk, commit->parents[i], commit->uninteresting);

	return error;
}

static int push_commit(git_revwalk *walk, const git_oid *oid, int uninteresting)
{
	commit_object *commit;

	commit = commit_lookup(walk, oid);
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

	if (git_reference_name_to_oid(&oid, walk->repo, refname) < 0)
		return -1;

	return push_commit(walk, &oid, hide);
}

struct push_cb_data {
	git_revwalk *walk;
	const char *glob;
	int hide;
};

static int push_glob_cb(const char *refname, void *data_)
{
	struct push_cb_data *data = (struct push_cb_data *)data_;

	if (!p_fnmatch(data->glob, refname, 0))
		return push_ref(data->walk, refname, data->hide);

	return 0;
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
	data.glob = git_buf_cstr(&buf);
	data.hide = hide;

	if (git_reference_foreach(
			walk->repo, GIT_REF_LISTALL, push_glob_cb, &data) < 0)
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

int git_revwalk_hide_ref(git_revwalk *walk, const char *refname)
{
	assert(walk && refname);
	return push_ref(walk, refname, 1);
}

static int revwalk_enqueue_timesort(git_revwalk *walk, commit_object *commit)
{
	return git_pqueue_insert(&walk->iterator_time, commit);
}

static int revwalk_enqueue_unsorted(git_revwalk *walk, commit_object *commit)
{
	return commit_list_insert(commit, &walk->iterator_rand) ? 0 : -1;
}

static int revwalk_next_timesort(commit_object **object_out, git_revwalk *walk)
{
	int error;
	commit_object *next;

	while ((next = git_pqueue_pop(&walk->iterator_time)) != NULL) {
		if ((error = process_commit_parents(walk, next)) < 0)
			return error;

		if (!next->uninteresting) {
			*object_out = next;
			return 0;
		}
	}

	return GIT_REVWALKOVER;
}

static int revwalk_next_unsorted(commit_object **object_out, git_revwalk *walk)
{
	int error;
	commit_object *next;

	while ((next = commit_list_pop(&walk->iterator_rand)) != NULL) {
		if ((error = process_commit_parents(walk, next)) < 0)
			return error;

		if (!next->uninteresting) {
			*object_out = next;
			return 0;
		}
	}

	return GIT_REVWALKOVER;
}

static int revwalk_next_toposort(commit_object **object_out, git_revwalk *walk)
{
	commit_object *next;
	unsigned short i;

	for (;;) {
		next = commit_list_pop(&walk->iterator_topo);
		if (next == NULL)
			return GIT_REVWALKOVER;

		if (next->in_degree > 0) {
			next->topo_delay = 1;
			continue;
		}

		for (i = 0; i < next->out_degree; ++i) {
			commit_object *parent = next->parents[i];

			if (--parent->in_degree == 0 && parent->topo_delay) {
				parent->topo_delay = 0;
				if (commit_list_insert(parent, &walk->iterator_topo) == NULL)
					return -1;
			}
		}

		*object_out = next;
		return 0;
	}
}

static int revwalk_next_reverse(commit_object **object_out, git_revwalk *walk)
{
	*object_out = commit_list_pop(&walk->iterator_reverse);
	return *object_out ? 0 : GIT_REVWALKOVER;
}


static int prepare_walk(git_revwalk *walk)
{
	int error;
	unsigned int i;
	commit_object *next, *two;
	commit_list *bases = NULL;

	/*
	 * If walk->one is NULL, there were no positive references,
	 * so we know that the walk is already over.
	 */
	if (walk->one == NULL)
		return GIT_REVWALKOVER;

	/* first figure out what the merge bases are */
	if (merge_bases_many(&bases, walk, walk->one, &walk->twos) < 0)
		return -1;

	commit_list_free(&bases);
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
				commit_object *parent = next->parents[i];
				parent->in_degree++;
			}

			if (commit_list_insert(next, &walk->iterator_topo) == NULL)
				return -1;
		}

		if (error != GIT_REVWALKOVER)
			return error;

		walk->get_next = &revwalk_next_toposort;
	}

	if (walk->sorting & GIT_SORT_REVERSE) {

		while ((error = walk->get_next(&next, walk)) == 0)
			if (commit_list_insert(next, &walk->iterator_reverse) == NULL)
				return -1;

		if (error != GIT_REVWALKOVER)
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

	if (git_pqueue_init(&walk->iterator_time, 8, commit_time_cmp) < 0 ||
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

int git_revwalk_next(git_oid *oid, git_revwalk *walk)
{
	int error;
	commit_object *next;

	assert(walk && oid);

	if (!walk->walking) {
		if ((error = prepare_walk(walk)) < 0)
			return error;
	}

	error = walk->get_next(&next, walk);

	if (error == GIT_REVWALKOVER) {
		git_revwalk_reset(walk);
		return GIT_REVWALKOVER;
	}

	if (!error)
		git_oid_cpy(oid, &next->oid);

	return error;
}

void git_revwalk_reset(git_revwalk *walk)
{
	commit_object *commit;

	assert(walk);

	kh_foreach_value(walk->commits, commit, {
		commit->seen = 0;
		commit->in_degree = 0;
		commit->topo_delay = 0;
		commit->uninteresting = 0;
		});

	git_pqueue_clear(&walk->iterator_time);
	commit_list_free(&walk->iterator_topo);
	commit_list_free(&walk->iterator_rand);
	commit_list_free(&walk->iterator_reverse);
	walk->walking = 0;

	walk->one = NULL;
	git_vector_clear(&walk->twos);
}

