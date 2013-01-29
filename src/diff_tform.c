/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#include "common.h"
#include "diff.h"
#include "git2/config.h"

static git_diff_delta *diff_delta__dup(
	const git_diff_delta *d, git_pool *pool)
{
	git_diff_delta *delta = git__malloc(sizeof(git_diff_delta));
	if (!delta)
		return NULL;

	memcpy(delta, d, sizeof(git_diff_delta));

	delta->old_file.path = git_pool_strdup(pool, d->old_file.path);
	if (delta->old_file.path == NULL)
		goto fail;

	if (d->new_file.path != d->old_file.path) {
		delta->new_file.path = git_pool_strdup(pool, d->new_file.path);
		if (delta->new_file.path == NULL)
			goto fail;
	} else {
		delta->new_file.path = delta->old_file.path;
	}

	return delta;

fail:
	git__free(delta);
	return NULL;
}

static git_diff_delta *diff_delta__merge_like_cgit(
	const git_diff_delta *a, const git_diff_delta *b, git_pool *pool)
{
	git_diff_delta *dup;

	/* Emulate C git for merging two diffs (a la 'git diff <sha>').
	 *
	 * When C git does a diff between the work dir and a tree, it actually
	 * diffs with the index but uses the workdir contents.  This emulates
	 * those choices so we can emulate the type of diff.
	 *
	 * We have three file descriptions here, let's call them:
	 *  f1 = a->old_file
	 *  f2 = a->new_file AND b->old_file
	 *  f3 = b->new_file
	 */

	/* if f2 == f3 or f2 is deleted, then just dup the 'a' diff */
	if (b->status == GIT_DELTA_UNMODIFIED || a->status == GIT_DELTA_DELETED)
		return diff_delta__dup(a, pool);

	/* otherwise, base this diff on the 'b' diff */
	if ((dup = diff_delta__dup(b, pool)) == NULL)
		return NULL;

	/* If 'a' status is uninteresting, then we're done */
	if (a->status == GIT_DELTA_UNMODIFIED)
		return dup;

	assert(a->status != GIT_DELTA_UNMODIFIED);
	assert(b->status != GIT_DELTA_UNMODIFIED);

	/* A cgit exception is that the diff of a file that is only in the
	 * index (i.e. not in HEAD nor workdir) is given as empty.
	 */
	if (dup->status == GIT_DELTA_DELETED) {
		if (a->status == GIT_DELTA_ADDED)
			dup->status = GIT_DELTA_UNMODIFIED;
		/* else don't overwrite DELETE status */
	} else {
		dup->status = a->status;
	}

	git_oid_cpy(&dup->old_file.oid, &a->old_file.oid);
	dup->old_file.mode  = a->old_file.mode;
	dup->old_file.size  = a->old_file.size;
	dup->old_file.flags = a->old_file.flags;

	return dup;
}

int git_diff_merge(
	git_diff_list *onto,
	const git_diff_list *from)
{
	int error = 0;
	git_pool onto_pool;
	git_vector onto_new;
	git_diff_delta *delta;
	bool ignore_case = false;
	unsigned int i, j;

	assert(onto && from);

	if (!from->deltas.length)
		return 0;

	if (git_vector_init(
			&onto_new, onto->deltas.length, git_diff_delta__cmp) < 0 ||
		git_pool_init(&onto_pool, 1, 0) < 0)
		return -1;

	if ((onto->opts.flags & GIT_DIFF_DELTAS_ARE_ICASE) != 0 ||
		(from->opts.flags & GIT_DIFF_DELTAS_ARE_ICASE) != 0)
	{
		ignore_case = true;

		/* This function currently only supports merging diff lists that
		 * are sorted identically. */
		assert((onto->opts.flags & GIT_DIFF_DELTAS_ARE_ICASE) != 0 &&
				(from->opts.flags & GIT_DIFF_DELTAS_ARE_ICASE) != 0);
	}

	for (i = 0, j = 0; i < onto->deltas.length || j < from->deltas.length; ) {
		git_diff_delta *o = GIT_VECTOR_GET(&onto->deltas, i);
		const git_diff_delta *f = GIT_VECTOR_GET(&from->deltas, j);
		int cmp = !f ? -1 : !o ? 1 : STRCMP_CASESELECT(ignore_case, o->old_file.path, f->old_file.path);

		if (cmp < 0) {
			delta = diff_delta__dup(o, &onto_pool);
			i++;
		} else if (cmp > 0) {
			delta = diff_delta__dup(f, &onto_pool);
			j++;
		} else {
			delta = diff_delta__merge_like_cgit(o, f, &onto_pool);
			i++;
			j++;
		}

		/* the ignore rules for the target may not match the source
		 * or the result of a merged delta could be skippable...
		 */
		if (git_diff_delta__should_skip(&onto->opts, delta)) {
			git__free(delta);
			continue;
		}

		if ((error = !delta ? -1 : git_vector_insert(&onto_new, delta)) < 0)
			break;
	}

	if (!error) {
		git_vector_swap(&onto->deltas, &onto_new);
		git_pool_swap(&onto->pool, &onto_pool);
		onto->new_src = from->new_src;

		/* prefix strings also come from old pool, so recreate those.*/
		onto->opts.old_prefix =
			git_pool_strdup_safe(&onto->pool, onto->opts.old_prefix);
		onto->opts.new_prefix =
			git_pool_strdup_safe(&onto->pool, onto->opts.new_prefix);
	}

	git_vector_foreach(&onto_new, i, delta)
		git__free(delta);
	git_vector_free(&onto_new);
	git_pool_clear(&onto_pool);

	return error;
}

#define DEFAULT_THRESHOLD 50
#define DEFAULT_BREAK_REWRITE_THRESHOLD 60
#define DEFAULT_TARGET_LIMIT 200

static int normalize_find_opts(
	git_diff_list *diff,
	git_diff_find_options *opts,
	git_diff_find_options *given)
{
	git_config *cfg = NULL;
	const char *val;

	if (diff->repo != NULL &&
		git_repository_config__weakptr(&cfg, diff->repo) < 0)
		return -1;

	if (given != NULL)
		memcpy(opts, given, sizeof(*opts));
	else {
		git_diff_find_options init = GIT_DIFF_FIND_OPTIONS_INIT;
		memmove(opts, &init, sizeof(init));

		opts->flags = GIT_DIFF_FIND_RENAMES;

		if (git_config_get_string(&val, cfg, "diff.renames") < 0)
			giterr_clear();
		else if (val &&
			(!strcasecmp(val, "copies") || !strcasecmp(val, "copy")))
			opts->flags = GIT_DIFF_FIND_RENAMES | GIT_DIFF_FIND_COPIES;
	}

	GITERR_CHECK_VERSION(opts, GIT_DIFF_FIND_OPTIONS_VERSION, "git_diff_find_options");

	/* some flags imply others */

	if (opts->flags & GIT_DIFF_FIND_RENAMES_FROM_REWRITES)
		opts->flags |= GIT_DIFF_FIND_RENAMES;

	if (opts->flags & GIT_DIFF_FIND_COPIES_FROM_UNMODIFIED)
		opts->flags |= GIT_DIFF_FIND_COPIES;

#define USE_DEFAULT(X) ((X) == 0 || (X) > 100)

	if (USE_DEFAULT(opts->rename_threshold))
		opts->rename_threshold = DEFAULT_THRESHOLD;

	if (USE_DEFAULT(opts->rename_from_rewrite_threshold))
		opts->rename_from_rewrite_threshold = DEFAULT_THRESHOLD;

	if (USE_DEFAULT(opts->copy_threshold))
		opts->copy_threshold = DEFAULT_THRESHOLD;

	if (USE_DEFAULT(opts->break_rewrite_threshold))
		opts->break_rewrite_threshold = DEFAULT_BREAK_REWRITE_THRESHOLD;

#undef USE_DEFAULT

	if (!opts->target_limit) {
		int32_t limit = 0;

		opts->target_limit = DEFAULT_TARGET_LIMIT;

		if (git_config_get_int32(&limit, cfg, "diff.renameLimit") < 0)
			giterr_clear();
		else if (limit > 0)
			opts->target_limit = limit;
	}

	return 0;
}

static int apply_splits_and_deletes(git_diff_list *diff, size_t expected_size)
{
	git_vector onto = GIT_VECTOR_INIT;
	size_t i;
	git_diff_delta *delta;

	if (git_vector_init(&onto, expected_size, git_diff_delta__cmp) < 0)
		return -1;

	/* build new delta list without TO_DELETE and splitting TO_SPLIT */
	git_vector_foreach(&diff->deltas, i, delta) {
		if (delta->status == GIT_DELTA__TO_DELETE)
			continue;

		if (delta->status == GIT_DELTA__TO_SPLIT) {
			git_diff_delta *deleted = diff_delta__dup(delta, &diff->pool);
			if (!deleted)
				goto on_error;

			deleted->status = GIT_DELTA_DELETED;
			memset(&deleted->new_file, 0, sizeof(deleted->new_file));
			deleted->new_file.path = deleted->old_file.path;
			deleted->new_file.flags |= GIT_DIFF_FILE_VALID_OID;

			if (git_vector_insert(&onto, deleted) < 0)
				goto on_error;

			delta->status = GIT_DELTA_ADDED;
			memset(&delta->old_file, 0, sizeof(delta->old_file));
			delta->old_file.path = delta->new_file.path;
			delta->old_file.flags |= GIT_DIFF_FILE_VALID_OID;
		}

		if (git_vector_insert(&onto, delta) < 0)
			goto on_error;
	}

	/* cannot return an error past this point */
	git_vector_foreach(&diff->deltas, i, delta)
		if (delta->status == GIT_DELTA__TO_DELETE)
			git__free(delta);

	/* swap new delta list into place */
	git_vector_sort(&onto);
	git_vector_swap(&diff->deltas, &onto);
	git_vector_free(&onto);

	return 0;

on_error:
	git_vector_foreach(&onto, i, delta)
		git__free(delta);

	git_vector_free(&onto);

	return -1;
}

static unsigned int calc_similarity(
	void *cache, git_diff_file *old_file, git_diff_file *new_file)
{
	GIT_UNUSED(cache);

	if (git_oid_cmp(&old_file->oid, &new_file->oid) == 0)
		return 100;

	/* TODO: insert actual similarity algo here */

	return 0;
}

#define FLAG_SET(opts,flag_name) ((opts.flags & flag_name) != 0)

int git_diff_find_similar(
	git_diff_list *diff,
	git_diff_find_options *given_opts)
{
	unsigned int i, j, similarity;
	git_diff_delta *from, *to;
	git_diff_find_options opts;
	unsigned int tried_targets, num_changes = 0;
	git_vector matches = GIT_VECTOR_INIT;

	if (normalize_find_opts(diff, &opts, given_opts) < 0)
		return -1;

	/* first do splits if requested */

	if (FLAG_SET(opts, GIT_DIFF_FIND_AND_BREAK_REWRITES)) {
		git_vector_foreach(&diff->deltas, i, from) {
			if (from->status != GIT_DELTA_MODIFIED)
				continue;

			/* Right now, this doesn't work right because the similarity
			 * algorithm isn't actually implemented...
			 */
			similarity = 100;
			/* calc_similarity(NULL, &from->old_file, from->new_file); */

			if (similarity < opts.break_rewrite_threshold) {
				from->status = GIT_DELTA__TO_SPLIT;
				num_changes++;
			}
		}

		/* apply splits as needed */
		if (num_changes > 0 &&
			apply_splits_and_deletes(
				diff, diff->deltas.length + num_changes) < 0)
			return -1;
	}

	/* next find the most similar delta for each rename / copy candidate */

	if (git_vector_init(&matches, diff->deltas.length, git_diff_delta__cmp) < 0)
		return -1;

	git_vector_foreach(&diff->deltas, i, from) {
		tried_targets = 0;

		git_vector_foreach(&diff->deltas, j, to) {
			if (i == j)
				continue;

			switch (to->status) {
			case GIT_DELTA_ADDED:
			case GIT_DELTA_UNTRACKED:
			case GIT_DELTA_RENAMED:
			case GIT_DELTA_COPIED:
				break;
			default:
				/* only the above status values should be checked */
				continue;
			}

			/* skip all but DELETED files unless copy detection is on */
			if (from->status != GIT_DELTA_DELETED &&
				!FLAG_SET(opts, GIT_DIFF_FIND_COPIES))
				continue;

			/* don't check UNMODIFIED files as source unless given option */
			if (from->status == GIT_DELTA_UNMODIFIED &&
				!FLAG_SET(opts, GIT_DIFF_FIND_COPIES_FROM_UNMODIFIED))
				continue;

			/* cap on maximum files we'll examine */
			if (++tried_targets > opts.target_limit)
				break;

			/* calculate similarity and see if this pair beats the
			 * similarity score of the current best pair.
			 */
			similarity = calc_similarity(NULL, &from->old_file, &to->new_file);

			if (to->similarity < similarity) {
				to->similarity = similarity;
				if (git_vector_set(NULL, &matches, j, from) < 0)
					return -1;
			}
		}
	}

	/* next rewrite the diffs with renames / copies */

	num_changes = 0;

	git_vector_foreach(&diff->deltas, j, to) {
		from = GIT_VECTOR_GET(&matches, j);
		if (!from) {
			assert(to->similarity == 0);
			continue;
		}

		/* three possible outcomes here:
		 * 1. old DELETED and if over rename threshold,
		 *    new becomes RENAMED and old goes away
		 * 2. old was MODIFIED but FIND_RENAMES_FROM_REWRITES is on and
		 *    old is more similar to new than it is to itself, in which
		 *    case, new becomes RENAMED and old becomed ADDED
		 * 3. otherwise if over copy threshold, new becomes COPIED
		 */

		if (from->status == GIT_DELTA_DELETED) {
			if (to->similarity < opts.rename_threshold) {
				to->similarity = 0;
				continue;
			}

			to->status = GIT_DELTA_RENAMED;
			memcpy(&to->old_file, &from->old_file, sizeof(to->old_file));

			from->status = GIT_DELTA__TO_DELETE;
			num_changes++;

			continue;
		}

		if (from->status == GIT_DELTA_MODIFIED &&
			FLAG_SET(opts, GIT_DIFF_FIND_RENAMES_FROM_REWRITES) &&
			to->similarity > opts.rename_threshold)
		{
			similarity = 100;
			/* calc_similarity(NULL, &from->old_file, from->new_file); */

			if (similarity < opts.rename_from_rewrite_threshold) {
				to->status = GIT_DELTA_RENAMED;
				memcpy(&to->old_file, &from->old_file, sizeof(to->old_file));

				from->status = GIT_DELTA_ADDED;
				memset(&from->old_file, 0, sizeof(from->old_file));
				from->old_file.path = to->old_file.path;
				from->old_file.flags |= GIT_DIFF_FILE_VALID_OID;

				continue;
			}
		}

		if (to->similarity < opts.copy_threshold) {
			to->similarity = 0;
			continue;
		}

		/* convert "to" to a COPIED record */
		to->status = GIT_DELTA_COPIED;
		memcpy(&to->old_file, &from->old_file, sizeof(to->old_file));
	}

	git_vector_free(&matches);

	if (num_changes > 0) {
		assert(num_changes < diff->deltas.length);

		if (apply_splits_and_deletes(
				diff, diff->deltas.length - num_changes) < 0)
			return -1;
	}

	return 0;
}

#undef FLAG_SET
