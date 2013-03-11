/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#include "common.h"
#include "diff.h"
#include "git2/config.h"
#include "git2/blob.h"
#include "hashsig.h"

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

static int find_similar__hashsig_for_file(
	void **out, const git_diff_file *f, const char *path, void *p)
{
	git_hashsig_option_t opt = (git_hashsig_option_t)p;
	int error = 0;

	GIT_UNUSED(f);
	error = git_hashsig_create_fromfile((git_hashsig **)out, path, opt);
	
	if (error == GIT_EBUFS) {
		error = 0;
		giterr_clear();
	}

	return error;
}

static int find_similar__hashsig_for_buf(
	void **out, const git_diff_file *f, const char *buf, size_t len, void *p)
{
	git_hashsig_option_t opt = (git_hashsig_option_t)p;
	int error = 0;
	
	GIT_UNUSED(f);
	error = git_hashsig_create((git_hashsig **)out, buf, len, opt);
	
	if (error == GIT_EBUFS) {
		error = 0;
		giterr_clear();
	}

	return error;
}

static void find_similar__hashsig_free(void *sig, void *payload)
{
	GIT_UNUSED(payload);
	git_hashsig_free(sig);
}

static int find_similar__calc_similarity(
	int *score, void *siga, void *sigb, void *payload)
{
	GIT_UNUSED(payload);
	*score = git_hashsig_compare(siga, sigb);
	return 0;
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

	if (diff->repo != NULL &&
		git_repository_config__weakptr(&cfg, diff->repo) < 0)
		return -1;

	if (given != NULL)
		memcpy(opts, given, sizeof(*opts));
	else {
		const char *val = NULL;

		GIT_INIT_STRUCTURE(opts, GIT_DIFF_FIND_OPTIONS_VERSION);

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

	/* assign the internal metric with whitespace flag as payload */
	if (!opts->metric) {
		opts->metric = git__malloc(sizeof(git_diff_similarity_metric));
		GITERR_CHECK_ALLOC(opts->metric);

		opts->metric->file_signature = find_similar__hashsig_for_file;
		opts->metric->buffer_signature = find_similar__hashsig_for_buf;
		opts->metric->free_signature = find_similar__hashsig_free;
		opts->metric->similarity = find_similar__calc_similarity;

		if (opts->flags & GIT_DIFF_FIND_IGNORE_WHITESPACE)
			opts->metric->payload = (void *)GIT_HASHSIG_IGNORE_WHITESPACE;
		else if (opts->flags & GIT_DIFF_FIND_DONT_IGNORE_WHITESPACE)
			opts->metric->payload = (void *)GIT_HASHSIG_NORMAL;
		else
			opts->metric->payload = (void *)GIT_HASHSIG_SMART_WHITESPACE;
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
		if ((delta->flags & GIT_DIFF_FLAG__TO_DELETE) != 0)
			continue;

		if ((delta->flags & GIT_DIFF_FLAG__TO_SPLIT) != 0) {
			git_diff_delta *deleted = diff_delta__dup(delta, &diff->pool);
			if (!deleted)
				goto on_error;

			deleted->status = GIT_DELTA_DELETED;
			memset(&deleted->new_file, 0, sizeof(deleted->new_file));
			deleted->new_file.path = deleted->old_file.path;
			deleted->new_file.flags |= GIT_DIFF_FLAG_VALID_OID;

			if (git_vector_insert(&onto, deleted) < 0)
				goto on_error;

			delta->status = GIT_DELTA_ADDED;
			memset(&delta->old_file, 0, sizeof(delta->old_file));
			delta->old_file.path = delta->new_file.path;
			delta->old_file.flags |= GIT_DIFF_FLAG_VALID_OID;
		}

		if (git_vector_insert(&onto, delta) < 0)
			goto on_error;
	}

	/* cannot return an error past this point */
	git_vector_foreach(&diff->deltas, i, delta)
		if ((delta->flags & GIT_DIFF_FLAG__TO_DELETE) != 0)
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

GIT_INLINE(git_diff_file *) similarity_get_file(git_diff_list *diff, size_t idx)
{
	git_diff_delta *delta = git_vector_get(&diff->deltas, idx / 2);
	return (idx & 1) ? &delta->new_file : &delta->old_file;
}

static int similarity_calc(
	git_diff_list *diff,
	git_diff_find_options *opts,
	size_t file_idx,
	void **cache)
{
	int error = 0;
	git_diff_file *file = similarity_get_file(diff, file_idx);
	git_iterator_type_t src = (file_idx & 1) ? diff->old_src : diff->new_src;

	if (src == GIT_ITERATOR_TYPE_WORKDIR) { /* compute hashsig from file */
		git_buf path = GIT_BUF_INIT;

		/* TODO: apply wd-to-odb filters to file data if necessary */

		if (!(error = git_buf_joinpath(
				&path, git_repository_workdir(diff->repo), file->path)))
			error = opts->metric->file_signature(
				&cache[file_idx], file, path.ptr, opts->metric->payload);

		git_buf_free(&path);
	} else { /* compute hashsig from blob buffer */
		git_blob *blob = NULL;

		/* TODO: add max size threshold a la diff? */

		if ((error = git_blob_lookup(&blob, diff->repo, &file->oid)) < 0)
			return error;

		error = opts->metric->buffer_signature(
			&cache[file_idx], file, git_blob_rawcontent(blob),
			git_blob_rawsize(blob), opts->metric->payload);

		git_blob_free(blob);
	}

	return error;
}

static int similarity_measure(
	git_diff_list *diff,
	git_diff_find_options *opts,
	void **cache,
	size_t a_idx,
	size_t b_idx)
{
	int score = 0;
	git_diff_file *a_file = similarity_get_file(diff, a_idx);
	git_diff_file *b_file = similarity_get_file(diff, b_idx);

	if (GIT_MODE_TYPE(a_file->mode) != GIT_MODE_TYPE(b_file->mode))
		return 0;

	if (git_oid_cmp(&a_file->oid, &b_file->oid) == 0)
		return 100;

	/* update signature cache if needed */
	if (!cache[a_idx] && similarity_calc(diff, opts, a_idx, cache) < 0)
		return -1;
	if (!cache[b_idx] && similarity_calc(diff, opts, b_idx, cache) < 0)
		return -1;
	
	/* some metrics may not wish to process this file (too big / too small) */
	if (!cache[a_idx] || !cache[b_idx])
		return 0;

	/* compare signatures */
	if (opts->metric->similarity(
			&score, cache[a_idx], cache[b_idx], opts->metric->payload) < 0)
		return -1;

	/* clip score */
	if (score < 0)
		score = 0;
	else if (score > 100)
		score = 100;

	return score;
}

#define FLAG_SET(opts,flag_name) ((opts.flags & flag_name) != 0)

int git_diff_find_similar(
	git_diff_list *diff,
	git_diff_find_options *given_opts)
{
	size_t i, j, cache_size, *matches;
	int error = 0, similarity;
	git_diff_delta *from, *to;
	git_diff_find_options opts;
	size_t tried_targets, num_rewrites = 0;
	void **cache;

	if ((error = normalize_find_opts(diff, &opts, given_opts)) < 0)
		return error;

	/* TODO: maybe abort if deltas.length > target_limit ??? */

	cache_size = diff->deltas.length * 2; /* must store b/c length may change */
	cache = git__calloc(cache_size, sizeof(void *));
	GITERR_CHECK_ALLOC(cache);

	matches = git__calloc(diff->deltas.length, sizeof(size_t));
	GITERR_CHECK_ALLOC(matches);

	/* first break MODIFIED records that are too different (if requested) */

	if (FLAG_SET(opts, GIT_DIFF_FIND_AND_BREAK_REWRITES)) {
		git_vector_foreach(&diff->deltas, i, from) {
			if (from->status != GIT_DELTA_MODIFIED)
				continue;

			similarity = similarity_measure(
				diff, &opts, cache, 2 * i, 2 * i + 1);

			if (similarity < 0) {
				error = similarity;
				goto cleanup;
			}

			if ((unsigned int)similarity < opts.break_rewrite_threshold) {
				from->flags |= GIT_DIFF_FLAG__TO_SPLIT;
				num_rewrites++;
			}
		}
	}

	/* next find the most similar delta for each rename / copy candidate */

	git_vector_foreach(&diff->deltas, i, from) {
		tried_targets = 0;

		/* skip things that aren't blobs */
		if (GIT_MODE_TYPE(from->old_file.mode) !=
			GIT_MODE_TYPE(GIT_FILEMODE_BLOB))
			continue;

		/* don't check UNMODIFIED files as source unless given option */
		if (from->status == GIT_DELTA_UNMODIFIED &&
			!FLAG_SET(opts, GIT_DIFF_FIND_COPIES_FROM_UNMODIFIED))
			continue;

		/* skip all but DELETED files unless copy detection is on */
		if (!FLAG_SET(opts, GIT_DIFF_FIND_COPIES) &&
			from->status != GIT_DELTA_DELETED &&
			(from->flags & GIT_DIFF_FLAG__TO_SPLIT) == 0)
			continue;

		git_vector_foreach(&diff->deltas, j, to) {
			if (i == j)
				continue;

			/* skip things that aren't blobs */
			if (GIT_MODE_TYPE(to->new_file.mode) !=
				GIT_MODE_TYPE(GIT_FILEMODE_BLOB))
				continue;

			switch (to->status) {
			case GIT_DELTA_ADDED:
			case GIT_DELTA_UNTRACKED:
			case GIT_DELTA_RENAMED:
			case GIT_DELTA_COPIED:
				break;
			case GIT_DELTA_MODIFIED:
				if ((to->flags & GIT_DIFF_FLAG__TO_SPLIT) == 0)
					continue;
				break;
			default:
				/* only the above status values should be checked */
				continue;
			}

			/* cap on maximum files we'll examine (per "from" file) */
			if (++tried_targets > opts.target_limit)
				break;

			/* calculate similarity and see if this pair beats the
			 * similarity score of the current best pair.
			 */
			similarity = similarity_measure(
				diff, &opts, cache, 2 * i, 2 * j + 1);

			if (similarity < 0) {
				error = similarity;
				goto cleanup;
			}

			if (to->similarity < (unsigned int)similarity) {
				to->similarity = (unsigned int)similarity;
				matches[j] = i + 1;
			}
		}
	}

	/* next rewrite the diffs with renames / copies */

	git_vector_foreach(&diff->deltas, j, to) {
		if (!matches[j]) {
			assert(to->similarity == 0);
			continue;
		}

		i = matches[j] - 1;
		from = GIT_VECTOR_GET(&diff->deltas, i);
		assert(from);

		/* four possible outcomes here:
		 * 1. old DELETED and if over rename threshold,
		 *    new becomes RENAMED and old goes away
		 * 2. old SPLIT and if over rename threshold,
		 *    new becomes RENAMED and old becomes ADDED (clear SPLIT)
		 * 3. old was MODIFIED but FIND_RENAMES_FROM_REWRITES is on and
		 *    old is more similar to new than it is to itself, in which
		 *    case, new becomes RENAMED and old becomed ADDED
		 * 4. otherwise if over copy threshold, new becomes COPIED
		 */

		if (from->status == GIT_DELTA_DELETED) {
			if (to->similarity < opts.rename_threshold) {
				to->similarity = 0;
				continue;
			}

			to->status = GIT_DELTA_RENAMED;
			memcpy(&to->old_file, &from->old_file, sizeof(to->old_file));

			from->flags |= GIT_DIFF_FLAG__TO_DELETE;
			num_rewrites++;

			continue;
		}

		if (from->status == GIT_DELTA_MODIFIED &&
			(from->flags & GIT_DIFF_FLAG__TO_SPLIT) != 0)
		{
			if (to->similarity < opts.rename_threshold) {
				to->similarity = 0;
				continue;
			}

			to->status = GIT_DELTA_RENAMED;
			memcpy(&to->old_file, &from->old_file, sizeof(to->old_file));

			from->status = GIT_DELTA_ADDED;
			from->flags &= ~GIT_DIFF_FLAG__TO_SPLIT;
			memset(&from->old_file, 0, sizeof(from->old_file));
			num_rewrites--;

			continue;
		}

		if (from->status == GIT_DELTA_MODIFIED &&
			FLAG_SET(opts, GIT_DIFF_FIND_RENAMES_FROM_REWRITES) &&
			to->similarity > opts.rename_threshold)
		{
			similarity = similarity_measure(
				diff, &opts, cache, 2 * i, 2 * i + 1);

			if (similarity < 0) {
				error = similarity;
				goto cleanup;
			}

			if ((unsigned int)similarity < opts.rename_from_rewrite_threshold) {
				to->status = GIT_DELTA_RENAMED;
				memcpy(&to->old_file, &from->old_file, sizeof(to->old_file));

				from->status = GIT_DELTA_ADDED;
				memset(&from->old_file, 0, sizeof(from->old_file));
				from->old_file.path = to->old_file.path;
				from->old_file.flags |= GIT_DIFF_FLAG_VALID_OID;

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

	if (num_rewrites > 0) {
		assert(num_rewrites < diff->deltas.length);

		error = apply_splits_and_deletes(
			diff, diff->deltas.length - num_rewrites);
	}

cleanup:
	git__free(matches);

	for (i = 0; i < cache_size; ++i) {
		if (cache[i] != NULL)
			opts.metric->free_signature(cache[i], opts.metric->payload);
	}
	git__free(cache);

	if (!given_opts || !given_opts->metric)
		git__free(opts.metric);

	return error;
}

#undef FLAG_SET
