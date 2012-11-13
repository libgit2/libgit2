/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#include "git2/describe.h"

#include "commit.h"
#include "commit_list.h"
#include "oidmap.h"
#include "refs.h"
#include "revwalk.h"
#include "tag.h"
#include "vector.h"

/* Ported from https://github.com/git/git/blob/89dde7882f71f846ccd0359756d27bebc31108de/builtin/describe.c */

struct commit_name {
	git_tag *tag;
	unsigned prio:2; /* annotated tag = 2, tag = 1, head = 0 */
	unsigned name_checked:1;
	git_oid sha1;
	char *path;

	/* Khash workaround. They original key has to still be reachable */
	git_oid peeled; 
};

static void *oidmap_value_bykey(git_oidmap *map, const git_oid *key)
{
	khint_t pos = git_oidmap_lookup_index(map, key);

	if (!git_oidmap_valid_index(map, pos))
		return NULL;

	return git_oidmap_value_at(map, pos);
}

static struct commit_name *find_commit_name(
	git_oidmap *names,
	const git_oid *peeled)
{
	return (struct commit_name *)(oidmap_value_bykey(names, peeled));
}

static int replace_name(
	git_tag **tag,
	git_repository *repo,
	struct commit_name *e,
	unsigned int prio,
	const git_oid *sha1)
{
	git_time_t e_time = 0, t_time = 0;

	if (!e || e->prio < prio)
		return 1;

	if (e->prio == 2 && prio == 2) {
		/* Multiple annotated tags point to the same commit.
		 * Select one to keep based upon their tagger date.
		 */
		git_tag *t = NULL;

		if (!e->tag) {
			if (git_tag_lookup(&t, repo, &e->sha1) < 0)
				return 1;
			e->tag = t;
		}

		if (git_tag_lookup(&t, repo, sha1) < 0)
			return 0;

		*tag = t;

		if (e->tag->tagger)
			e_time = e->tag->tagger->when.time;

		if (t->tagger)
			t_time = t->tagger->when.time;

		if (e_time < t_time)
			return 1;
	}

	return 0;
}

static int add_to_known_names(
	git_repository *repo,
	git_oidmap *names,
	const char *path,
	const git_oid *peeled,
	unsigned int prio,
	const git_oid *sha1)
{
	struct commit_name *e = find_commit_name(names, peeled);
	bool found = (e != NULL);

	git_tag *tag = NULL;
	if (replace_name(&tag, repo, e, prio, sha1)) {
		if (!found) {
			e = git__malloc(sizeof(struct commit_name));
			GITERR_CHECK_ALLOC(e);

			e->path = NULL;
			e->tag = NULL;
		}

		if (e->tag)
			git_tag_free(e->tag);
		e->tag = tag;
		e->prio = prio;
		e->name_checked = 0;
		git_oid_cpy(&e->sha1, sha1);
		git__free(e->path);
		e->path = git__strdup(path);
		git_oid_cpy(&e->peeled, peeled);

		if (!found) {
			int ret;

			git_oidmap_insert(names, &e->peeled, e, ret);
			if (ret < 0)
				return -1;
		}
	}
	else
		git_tag_free(tag);

	return 0;
}

static int retrieve_peeled_tag_or_object_oid(
	git_oid *peeled_out,
	git_oid *ref_target_out,
	git_repository *repo,
	const char *refname)
{
	git_reference *ref;
	git_object *peeled = NULL;
	int error;

	if ((error = git_reference_lookup_resolved(&ref, repo, refname, -1)) < 0)
		return error;

	if ((error = git_reference_peel(&peeled, ref, GIT_OBJ_ANY)) < 0)
		goto cleanup;

	git_oid_cpy(ref_target_out, git_reference_target(ref));
	git_oid_cpy(peeled_out, git_object_id(peeled));

	if (git_oid_cmp(ref_target_out, peeled_out) != 0)
		error = 1; /* The reference was pointing to a annotated tag */
	else
		error = 0; /* Any other object */

cleanup:
	git_reference_free(ref);
	git_object_free(peeled);
	return error;
}

struct get_name_data
{
	git_describe_opts *opts;
	git_repository *repo;
	git_oidmap *names;
};

static int get_name(const char *refname, void *payload)
{
	struct get_name_data *data;
	bool is_tag, is_annotated, all;
	git_oid peeled, sha1;
	unsigned int prio;
	int error = 0;

	data = (struct get_name_data *)payload;
	is_tag = !git__prefixcmp(refname, GIT_REFS_TAGS_DIR);
	all = data->opts->describe_strategy == GIT_DESCRIBE_ALL;

	/* Reject anything outside refs/tags/ unless --all */
	if (!all && !is_tag)
		return 0;

	/* Accept only tags that match the pattern, if given */
	if (data->opts->pattern && (!is_tag || p_fnmatch(data->opts->pattern,
		refname + strlen(GIT_REFS_TAGS_DIR), 0)))
				return 0;

	/* Is it annotated? */
	if ((error = retrieve_peeled_tag_or_object_oid(
		&peeled, &sha1, data->repo, refname)) < 0)
		return error;

	is_annotated = error;

	/*
	 * By default, we only use annotated tags, but with --tags
	 * we fall back to lightweight ones (even without --tags,
	 * we still remember lightweight ones, only to give hints
	 * in an error message).  --all allows any refs to be used.
	 */
	if (is_annotated)
		prio = 2;
	else if (is_tag)
		prio = 1;
	else
		prio = 0;

	add_to_known_names(data->repo, data->names,
		all ? refname + strlen(GIT_REFS_DIR) : refname + strlen(GIT_REFS_TAGS_DIR),
		&peeled, prio, &sha1);
	return 0;
}

struct possible_tag {
	struct commit_name *name;
	int depth;
	int found_order;
	unsigned flag_within;
};

static int compare_pt(const void *a_, const void *b_)
{
	struct possible_tag *a = (struct possible_tag *)a_;
	struct possible_tag *b = (struct possible_tag *)b_;
	if (a->depth != b->depth)
		return a->depth - b->depth;
	if (a->found_order != b->found_order)
		return a->found_order - b->found_order;
	return 0;
}

#define SEEN (1u << 0)

static unsigned long finish_depth_computation(
	git_pqueue *list,
	git_revwalk *walk,
	struct possible_tag *best)
{
	unsigned long seen_commits = 0;
	int error, i;

	while (git_pqueue_size(list) > 0) {
		git_commit_list_node *c = git_pqueue_pop(list);
		seen_commits++;
		if (c->flags & best->flag_within) {
			size_t index = 0;
			while (git_pqueue_size(list) > index) {
				git_commit_list_node *i = git_pqueue_peek_ahead(list, index);
				if (!(i->flags & best->flag_within))
					break;
				index++;
			}
			if (index > git_pqueue_size(list))
				break;
		} else
			best->depth++;
		for (i = 0; i < c->out_degree; i++) {
			git_commit_list_node *p = c->parents[i];
			if ((error = git_commit_list_parse(walk, p)) < 0)
				return error;
			if (!(p->flags & SEEN))
				if ((error = git_pqueue_insert(list, p)) < 0)
					return error;
			p->flags |= c->flags;
		}
	}
	return seen_commits;
}

static int display_name(git_buf *buf, git_repository *repo, struct commit_name *n)
{
	if (n->prio == 2 && !n->tag) {
		if (git_tag_lookup(&n->tag, repo, &n->sha1) < 0) {
			giterr_set(GITERR_TAG, "Annotated tag '%s' not available", n->path);
			return -1;
		}
	}

	if (n->tag && !n->name_checked) {
		if (!git_tag_name(n->tag)) {
			giterr_set(GITERR_TAG, "Annotated tag '%s' has no embedded name", n->path);
			return -1;
		}

		/* TODO: Cope with warnings
		if (strcmp(n->tag->tag, all ? n->path + 5 : n->path))
			warning(_("tag '%s' is really '%s' here"), n->tag->tag, n->path);
		*/

		n->name_checked = 1;
	}

	if (n->tag)
		git_buf_printf(buf, "%s", git_tag_name(n->tag));
	else
		git_buf_printf(buf, "%s", n->path);

	return 0;
}

static int find_unique_abbrev_size(
	int *out,
	const git_oid *oid_in,
	int abbreviated_size)
{
	GIT_UNUSED(abbreviated_size);

	/* TODO: Actually find the abbreviated oid size */
	GIT_UNUSED(oid_in);

	*out = GIT_OID_HEXSZ;
	
	return 0;
}

static int show_suffix(
	git_buf *buf,
	int depth,
	const git_oid* id,
	size_t abbrev_size)
{
	int error, size;

	char hex_oid[GIT_OID_HEXSZ];

	if ((error = find_unique_abbrev_size(&size, id, abbrev_size)) < 0)
		return error;

	git_oid_fmt(hex_oid, id);

	git_buf_printf(buf, "-%d-g", depth);

	git_buf_put(buf, hex_oid, size);
	git_buf_putc(buf, '\0');

	return git_buf_oom(buf) ? -1 : 0;
}

#define MAX_CANDIDATES_TAGS FLAG_BITS - 1

static int describe_not_found(const git_oid *oid, const char *message_format) {
	char oid_str[GIT_OID_HEXSZ + 1];
	git_oid_tostr(oid_str, sizeof(oid_str), oid);

	giterr_set(GITERR_DESCRIBE, message_format, oid_str);
	return GIT_ENOTFOUND;
}

static int describe(
	git_buf *out,
	struct get_name_data *data,
	git_commit *commit,
	const char *dirty_suffix)
{
	struct commit_name *n;
	struct possible_tag *best;
	bool all, tags;
	git_buf buf = GIT_BUF_INIT;
	git_revwalk *walk = NULL;
	git_pqueue list;
	git_commit_list_node *cmit, *gave_up_on = NULL;
	git_vector all_matches = GIT_VECTOR_INIT;
	unsigned int match_cnt = 0, annotated_cnt = 0, cur_match;
	unsigned long seen_commits = 0;	/* TODO: Check long */
	unsigned int unannotated_cnt = 0;
	int error;

	list.d = NULL;

	if (git_vector_init(&all_matches, MAX_CANDIDATES_TAGS, compare_pt) < 0)
		return -1;

	all = data->opts->describe_strategy == GIT_DESCRIBE_ALL;
	tags = data->opts->describe_strategy == GIT_DESCRIBE_TAGS;

	n = find_commit_name(data->names, git_commit_id(commit));
	if (n && (tags || all || n->prio == 2)) {
		/*
		 * Exact match to an existing ref.
		 */
		if ((error = display_name(&buf, data->repo, n)) < 0)
			goto cleanup;

		if (data->opts->always_use_long_format) {
			if ((error = show_suffix(&buf, 0,
				n->tag ? git_tag_target_id(n->tag) : git_commit_id(commit),
				data->opts->abbreviated_size)) < 0)
					goto cleanup;
		}

		if (dirty_suffix)
			git_buf_printf(&buf, "%s", dirty_suffix);

		if (git_buf_oom(&buf))
			return -1;

		goto found;
	}

	if (!data->opts->max_candidates_tags) {
		error = describe_not_found(
			git_commit_id(commit),
			"Cannot describe - no tag exactly matches '%s'");

		goto cleanup;
	}

	if ((error = git_revwalk_new(&walk, git_commit_owner(commit))) < 0)
		goto cleanup;

	if ((cmit = git_revwalk__commit_lookup(walk, git_commit_id(commit))) == NULL)
		goto cleanup;

	if ((error = git_commit_list_parse(walk, cmit)) < 0)
		goto cleanup;

	cmit->flags = SEEN;

	if ((error = git_pqueue_init(&list, 2, git_commit_list_time_cmp)) < 0)
		goto cleanup;

	if ((error = git_pqueue_insert(&list, cmit)) < 0)
		goto cleanup;

	while (git_pqueue_size(&list) > 0)
	{
		int i;

		git_commit_list_node *c = (git_commit_list_node *)git_pqueue_pop(&list);
		seen_commits++;

		n = find_commit_name(data->names, &c->oid);

		if (n) {
			if (!tags && !all && n->prio < 2) {
				unannotated_cnt++;
			} else if (match_cnt < data->opts->max_candidates_tags) {
				struct possible_tag *t = git__malloc(sizeof(struct commit_name));
				GITERR_CHECK_ALLOC(t);
				if ((error = git_vector_insert(&all_matches, t)) < 0)
					goto cleanup;

				match_cnt++;

				t->name = n;
				t->depth = seen_commits - 1;
				t->flag_within = 1u << match_cnt;
				t->found_order = match_cnt;
				c->flags |= t->flag_within;
				if (n->prio == 2)
					annotated_cnt++;
			}
			else {
				gave_up_on = c;
				break;
			}
		}

		for (cur_match = 0; cur_match < match_cnt; cur_match++) {
			struct possible_tag *t = git_vector_get(&all_matches, cur_match);
			if (!(c->flags & t->flag_within))
				t->depth++;
		}

		if (annotated_cnt && (git_pqueue_size(&list) == 0)) {
			/*
			if (debug) {
				char oid_str[GIT_OID_HEXSZ + 1];
				git_oid_tostr(oid_str, sizeof(oid_str), &c->oid);

				fprintf(stderr, "finished search at %s\n", oid_str);
			}
			*/
			break;
		}
		for (i = 0; i < c->out_degree; i++) {
			git_commit_list_node *p = c->parents[i];
			if ((error = git_commit_list_parse(walk, p)) < 0)
				goto cleanup;
			if (!(p->flags & SEEN))
				if ((error = git_pqueue_insert(&list, p)) < 0)
					goto cleanup;
			p->flags |= c->flags;

			if (data->opts->only_follow_first_parent)
				break;
		}
	}

	if (!match_cnt) {
		if (data->opts->show_commit_oid_as_fallback) {
			char hex_oid[GIT_OID_HEXSZ];
			int size;

			if ((error = find_unique_abbrev_size(
				&size, &cmit->oid, data->opts->abbreviated_size)) < 0)
					goto cleanup;

			git_oid_fmt(hex_oid, &cmit->oid);
			git_buf_put(&buf, hex_oid, size);

			if (dirty_suffix)
				git_buf_printf(&buf, "%s", dirty_suffix);

			if (git_buf_oom(&buf))
				return -1;

			goto found;
		}
		if (unannotated_cnt) {
			error = describe_not_found(git_commit_id(commit), 
				"Cannot describe - "
				"No annotated tags can describe '%s'."
			    "However, there were unannotated tags.");
			goto cleanup;
		}
		else {
			error = describe_not_found(git_commit_id(commit), 
				"Cannot describe - "
				"No tags can describe '%s'.");
			goto cleanup;
		}
	}

	best = (struct possible_tag *)git_vector_get(&all_matches, 0);

	git_vector_sort(&all_matches);

	best = (struct possible_tag *)git_vector_get(&all_matches, 0);

	if (gave_up_on) {
		git_pqueue_insert(&list, gave_up_on);
		seen_commits--;
	}
	if ((error = finish_depth_computation(
		&list, walk, best)) < 0)
		goto cleanup;
	seen_commits += error;

	/*
	{
		static const char *prio_names[] = {
			"head", "lightweight", "annotated",
		};

		char oid_str[GIT_OID_HEXSZ + 1];

		if (debug) {
			for (cur_match = 0; cur_match < match_cnt; cur_match++) {
				struct possible_tag *t = (struct possible_tag *)git_vector_get(&all_matches, cur_match);
				fprintf(stderr, " %-11s %8d %s\n",
					prio_names[t->name->prio],
					t->depth, t->name->path);
			}
			fprintf(stderr, "traversed %lu commits\n", seen_commits);
			if (gave_up_on) {
				git_oid_tostr(oid_str, sizeof(oid_str), &gave_up_on->oid);
				fprintf(stderr,
					"more than %i tags found; listed %i most recent\n"
					"gave up search at %s\n",
					data->opts->max_candidates_tags, data->opts->max_candidates_tags,
					oid_str);
			}
		}
	}
	*/

	if ((error = display_name(&buf, data->repo, best->name)) < 0)
		goto cleanup;

	if (data->opts->abbreviated_size) {
		if ((error = show_suffix(&buf, best->depth,
			&cmit->oid, data->opts->abbreviated_size)) < 0)
				goto cleanup;
	}

	if (dirty_suffix)
		git_buf_printf(&buf, "%s", dirty_suffix);

	if (git_buf_oom(&buf))
		return -1;

found:
	out->ptr = buf.ptr;
	out->asize = buf.asize;
	out->size = buf.size;

cleanup:
	{
		size_t i;
		struct possible_tag *match;
		git_vector_foreach(&all_matches, i, match) {
			git__free(match);
		}
	}
	git_vector_free(&all_matches);
	git_pqueue_free(&list);
	git_revwalk_free(walk);
	return error;
}

static int normalize_options(
	git_describe_opts *dst,
	const git_describe_opts *src)
{
	git_describe_opts default_options = GIT_DESCRIBE_OPTIONS_INIT;
	if (!src) src = &default_options;

	*dst = *src;

	if (dst->max_candidates_tags > GIT_DESCRIBE_DEFAULT_MAX_CANDIDATES_TAGS)
		dst->max_candidates_tags = GIT_DESCRIBE_DEFAULT_MAX_CANDIDATES_TAGS;

	if (dst->always_use_long_format && dst->abbreviated_size == 0) {
		giterr_set(GITERR_DESCRIBE, "Cannot describe - "
			"'always_use_long_format' is incompatible with a zero"
			"'abbreviated_size'");
		return -1;
	}

	return 0;
}

/** TODO: Add git_object_describe_workdir(git_buf *, const char *dirty_suffix, git_describe_opts *); */

int git_describe_object(
	git_buf *out,
	git_object *committish,
	git_describe_opts *opts)
{
	struct get_name_data data;
	struct commit_name *name;
	git_commit *commit;
	int error = -1;
	const char *dirty_suffix = NULL;
	git_describe_opts normOptions;

	assert(out && committish);

	data.opts = opts;
	data.repo = git_object_owner(committish);

	if ((error = normalize_options(&normOptions, opts)) < 0)
		return error;

	GITERR_CHECK_VERSION(
		&normOptions,
		GIT_DESCRIBE_OPTIONS_VERSION,
		"git_describe_opts");

	data.names = git_oidmap_alloc();
	GITERR_CHECK_ALLOC(data.names);

	/** TODO: contains to be implemented */

	/** TODO: deal with max_abbrev_size (either document or fix) */

	if ((error = git_object_peel((git_object **)(&commit), committish, GIT_OBJ_COMMIT)) < 0)
		goto cleanup;

	if (git_reference_foreach_name(
			git_object_owner(committish),
			get_name, &data) < 0)
				goto cleanup;

	if (git_oidmap_size(data.names) == 0) {
		giterr_set(GITERR_DESCRIBE, "Cannot describe - "
			"No reference found, cannot describe anything.");
		error = -1;
		goto cleanup;
	}

	if ((error = describe(out, &data, commit, dirty_suffix)) < 0)
		goto cleanup;

cleanup:
	git_oidmap_foreach_value(data.names, name, {
		git_tag_free(name->tag);
		git__free(name->path);
		git__free(name);
	});

	git_oidmap_free(data.names);
	git_commit_free(commit);

	return error;
}
