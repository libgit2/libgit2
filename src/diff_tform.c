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

	if (d->old_file.path != NULL) {
		delta->old_file.path = git_pool_strdup(pool, d->old_file.path);
		if (delta->old_file.path == NULL)
			goto fail;
	}

	if (d->new_file.path != d->old_file.path && d->new_file.path != NULL) {
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

int git_diff_find_similar__hashsig_for_file(
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

int git_diff_find_similar__hashsig_for_buf(
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

void git_diff_find_similar__hashsig_free(void *sig, void *payload)
{
	GIT_UNUSED(payload);
	git_hashsig_free(sig);
}

int git_diff_find_similar__calc_similarity(
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

	if (opts->flags & GIT_DIFF_FIND_EXACT_MATCH_ONLY) {
		/* if we are only looking for exact matches, then don't turn
		 * MODIFIED items into ADD/DELETE pairs because it's too picky
		 */
		opts->flags &= ~(GIT_DIFF_FIND_REWRITES | GIT_DIFF_BREAK_REWRITES);

		/* similarly, don't look for self-rewrites to split */
		opts->flags &= ~GIT_DIFF_FIND_RENAMES_FROM_REWRITES;
	}

	if (opts->flags & GIT_DIFF_FIND_RENAMES_FROM_REWRITES)
		opts->flags |= GIT_DIFF_FIND_RENAMES;

	if (opts->flags & GIT_DIFF_FIND_COPIES_FROM_UNMODIFIED)
		opts->flags |= GIT_DIFF_FIND_COPIES;

	if (opts->flags & GIT_DIFF_BREAK_REWRITES)
		opts->flags |= GIT_DIFF_FIND_REWRITES;

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

		opts->metric->file_signature = git_diff_find_similar__hashsig_for_file;
		opts->metric->buffer_signature = git_diff_find_similar__hashsig_for_buf;
		opts->metric->free_signature = git_diff_find_similar__hashsig_free;
		opts->metric->similarity = git_diff_find_similar__calc_similarity;

		if (opts->flags & GIT_DIFF_FIND_IGNORE_WHITESPACE)
			opts->metric->payload = (void *)GIT_HASHSIG_IGNORE_WHITESPACE;
		else if (opts->flags & GIT_DIFF_FIND_DONT_IGNORE_WHITESPACE)
			opts->metric->payload = (void *)GIT_HASHSIG_NORMAL;
		else
			opts->metric->payload = (void *)GIT_HASHSIG_SMART_WHITESPACE;
	}

	return 0;
}

static void validate_delta(git_diff_delta *delta)
{
	assert(delta);
	return;
/*
	switch (delta->status) {
	case GIT_DELTA_ADDED:
	case GIT_DELTA_UNTRACKED:
	case GIT_DELTA_IGNORED:
		assert(delta->new_file.path);
		break;
	case GIT_DELTA_DELETED:
		assert(delta->old_file.path);
		break;
	default:
		assert(delta->old_file.path && delta->new_file.path);
		break;
	}
*/
}

static int apply_splits_and_deletes(
	git_diff_list *diff, size_t expected_size, bool actually_split)
{
	git_vector onto = GIT_VECTOR_INIT;
	size_t i;
	git_diff_delta *delta, *deleted;

	if (git_vector_init(&onto, expected_size, git_diff_delta__cmp) < 0)
		return -1;

	/* build new delta list without TO_DELETE and splitting TO_SPLIT */
	git_vector_foreach(&diff->deltas, i, delta) {
		if ((delta->flags & GIT_DIFF_FLAG__TO_DELETE) != 0)
			continue;

		if ((delta->flags & GIT_DIFF_FLAG__TO_SPLIT) != 0) {

			/* just leave delta flagged with score if not actually splitting */
			if (!actually_split) {
				delta->flags = (delta->flags & ~GIT_DIFF_FLAG__TO_SPLIT);
				if (delta->status != GIT_DELTA_MODIFIED)
					delta->similarity = 0;
				continue;
			}

			delta->similarity = 0;

			/* make new record for DELETED side of split */
			if (!(deleted = diff_delta__dup(delta, &diff->pool)))
				goto on_error;

			deleted->status = GIT_DELTA_DELETED;
			memset(&deleted->new_file, 0, sizeof(deleted->new_file));
			deleted->new_file.path = deleted->old_file.path;
			deleted->new_file.flags |= GIT_DIFF_FLAG_VALID_OID;
			validate_delta(deleted);

			if (git_vector_insert(&onto, deleted) < 0)
				goto on_error;

			if (diff->new_src == GIT_ITERATOR_TYPE_WORKDIR)
				delta->status = GIT_DELTA_UNTRACKED;
			else
				delta->status = GIT_DELTA_ADDED;
			memset(&delta->old_file, 0, sizeof(delta->old_file));
			delta->old_file.path = delta->new_file.path;
			delta->old_file.flags |= GIT_DIFF_FLAG_VALID_OID;
			validate_delta(delta);
		}

		if (git_vector_insert(&onto, delta) < 0)
			goto on_error;
	}

	/* cannot return an error past this point */
	git_vector_foreach(&diff->deltas, i, delta)
		if ((delta->flags & GIT_DIFF_FLAG__TO_DELETE) != 0)
			git__free(delta);

	/* swap new delta list into place */
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
	git_iterator_type_t src = (file_idx & 1) ? diff->new_src : diff->old_src;

	if (src == GIT_ITERATOR_TYPE_WORKDIR) { /* compute hashsig from file */
		git_buf path = GIT_BUF_INIT;

		/* TODO: apply wd-to-odb filters to file data if necessary */

		if ((error = git_buf_joinpath(
				 &path, git_repository_workdir(diff->repo), file->path)) < 0)
			return error;

		/* if path is not a regular file, just skip this item */
		if (git_path_isfile(path.ptr))
			error = opts->metric->file_signature(
				&cache[file_idx], file, path.ptr, opts->metric->payload);

		git_buf_free(&path);
	} else { /* compute hashsig from blob buffer */
		git_blob *blob = NULL;
		git_off_t blobsize;

		/* TODO: add max size threshold a la diff? */

		if (git_blob_lookup(&blob, diff->repo, &file->oid) < 0) {
			/* if lookup fails, just skip this item in similarity calc */
			giterr_clear();
			return 0;
		}

		blobsize = git_blob_rawsize(blob);
		if (!git__is_sizet(blobsize)) /* ? what to do ? */
			blobsize = (size_t)-1;

		error = opts->metric->buffer_signature(
			&cache[file_idx], file, git_blob_rawcontent(blob),
			(size_t)blobsize, opts->metric->payload);

		git_blob_free(blob);
	}

	return error;
}

#define FLAG_SET(opts,flag_name) (((opts).flags & flag_name) != 0)

/* - score < 0 means files cannot be compared
 * - score >= 100 means files are exact match
 * - score == 0 means files are completely different
 */
static int similarity_measure(
	int *score,
	git_diff_list *diff,
	git_diff_find_options *opts,
	void **cache,
	size_t a_idx,
	size_t b_idx)
{
	git_diff_file *a_file = similarity_get_file(diff, a_idx);
	git_diff_file *b_file = similarity_get_file(diff, b_idx);
	bool exact_match = FLAG_SET(*opts, GIT_DIFF_FIND_EXACT_MATCH_ONLY);

	*score = -1;

	/* don't try to compare files of different types */
	if (GIT_MODE_TYPE(a_file->mode) != GIT_MODE_TYPE(b_file->mode))
		return 0;

	/* if exact match is requested, force calculation of missing OIDs */
	if (exact_match) {
		if (git_oid_iszero(&a_file->oid) &&
			diff->old_src == GIT_ITERATOR_TYPE_WORKDIR &&
			!git_diff__oid_for_file(diff->repo, a_file->path,
				a_file->mode, a_file->size, &a_file->oid))
			a_file->flags |= GIT_DIFF_FLAG_VALID_OID;

		if (git_oid_iszero(&b_file->oid) &&
			diff->new_src == GIT_ITERATOR_TYPE_WORKDIR &&
			!git_diff__oid_for_file(diff->repo, b_file->path,
				b_file->mode, b_file->size, &b_file->oid))
			b_file->flags |= GIT_DIFF_FLAG_VALID_OID;
	}

	/* check OID match as a quick test */
	if (git_oid__cmp(&a_file->oid, &b_file->oid) == 0) {
		*score = 100;
		return 0;
	}

	/* don't calculate signatures if we are doing exact match */
	if (exact_match) {
		*score = 0;
		return 0;
	}

	/* update signature cache if needed */
	if (!cache[a_idx] && similarity_calc(diff, opts, a_idx, cache) < 0)
		return -1;
	if (!cache[b_idx] && similarity_calc(diff, opts, b_idx, cache) < 0)
		return -1;

	/* some metrics may not wish to process this file (too big / too small) */
	if (!cache[a_idx] || !cache[b_idx])
		return 0;

	/* compare signatures */
	return opts->metric->similarity(
		score, cache[a_idx], cache[b_idx], opts->metric->payload);
}

static void convert_to_rename_and_add(
	git_diff_list *diff,
	git_diff_delta *from,
	git_diff_delta *to,
	int similarity)
{
	to->status = GIT_DELTA_RENAMED;
	to->flags &= ~GIT_DIFF_FLAG__TO_SPLIT; /* ensure no split */
	to->similarity = (uint32_t)similarity;
	memcpy(&to->old_file, &from->old_file, sizeof(to->old_file));
	validate_delta(to);

	if (diff->new_src == GIT_ITERATOR_TYPE_WORKDIR)
		from->status = GIT_DELTA_UNTRACKED;
	else
		from->status = GIT_DELTA_ADDED;
	from->flags &= ~GIT_DIFF_FLAG__TO_SPLIT; /* ensure no split */
	from->similarity = 0;
	memset(&from->old_file, 0, sizeof(from->old_file));
	from->old_file.path = from->new_file.path;
	from->old_file.flags |= GIT_DIFF_FLAG_VALID_OID;
	validate_delta(from);
}

typedef struct {
	uint32_t idx;
	uint32_t similarity;
} diff_find_match;

int git_diff_find_similar(
	git_diff_list *diff,
	git_diff_find_options *given_opts)
{
	size_t i, j, cache_size;
	int error = 0, similarity;
	git_diff_delta *from, *to;
	git_diff_find_options opts;
	size_t num_rewrites = 0, num_updates = 0;
	void **cache; /* cache of similarity metric file signatures */
	diff_find_match *matches; /* cache of best matches */

	if ((error = normalize_find_opts(diff, &opts, given_opts)) < 0)
		return error;

	/* TODO: maybe abort if deltas.length > target_limit ??? */
	if (!git__is_uint32(diff->deltas.length))
		return 0;

	cache_size = diff->deltas.length * 2; /* must store b/c length may change */
	cache = git__calloc(cache_size, sizeof(void *));
	GITERR_CHECK_ALLOC(cache);

	matches = git__calloc(diff->deltas.length, sizeof(diff_find_match));
	GITERR_CHECK_ALLOC(matches);

	/* first mark MODIFIED deltas to split if too different (if requested) */

	if (FLAG_SET(opts, GIT_DIFF_FIND_REWRITES)) {
		git_vector_foreach(&diff->deltas, i, from) {
			if (from->status != GIT_DELTA_MODIFIED)
				continue;

			/* skip things that aren't plain blobs */
			if (!GIT_MODE_ISBLOB(from->old_file.mode))
				continue;

			/* measure similarity from old_file to new_file */
			if ((error = similarity_measure(
					&similarity, diff, &opts, cache, 2 * i, 2 * i + 1)) < 0)
				goto cleanup;

			if (similarity < 0)
				continue;
			if (similarity < (int)opts.break_rewrite_threshold) {
				from->similarity = (uint32_t)similarity;
				from->flags |= GIT_DIFF_FLAG__TO_SPLIT;
				num_rewrites++;
			}
		}
	}

	/* next find the most similar delta for each rename / copy candidate */

	git_vector_foreach(&diff->deltas, i, from) {
		size_t tried_targets = 0;

		matches[i].idx = i;
		matches[i].similarity = 0;

		/* skip things that aren't plain blobs */
		if (!GIT_MODE_ISBLOB(from->old_file.mode))
			continue;

		/* don't check UNMODIFIED files as source unless given option */
		if (from->status == GIT_DELTA_UNMODIFIED &&
			!FLAG_SET(opts, GIT_DIFF_FIND_COPIES_FROM_UNMODIFIED))
			continue;

		/* don't check UNTRACKED files as source unless given option */
		if ((from->status == GIT_DELTA_UNTRACKED ||
			 from->status == GIT_DELTA_IGNORED) &&
			!FLAG_SET(opts, GIT_DIFF_FIND_FROM_UNTRACKED))
			continue;

		/* only use DELETED (or split MODIFIED) unless copy detection on */
		if (!FLAG_SET(opts, GIT_DIFF_FIND_COPIES) &&
			from->status != GIT_DELTA_DELETED &&
			(from->flags & GIT_DIFF_FLAG__TO_SPLIT) == 0)
			continue;

		git_vector_foreach(&diff->deltas, j, to) {
			if (i == j)
				continue;

			/* skip things that aren't blobs */
			if (!GIT_MODE_ISBLOB(to->new_file.mode))
				continue;

			/* only consider ADDED, RENAMED, COPIED, and split MODIFIED as
			 * targets; maybe include UNTRACKED and IGNORED if requested.
			 */
			switch (to->status) {
			case GIT_DELTA_ADDED:
			case GIT_DELTA_RENAMED:
			case GIT_DELTA_COPIED:
				break;
			case GIT_DELTA_MODIFIED:
				if ((to->flags & GIT_DIFF_FLAG__TO_SPLIT) == 0)
					continue;
				break;
			case GIT_DELTA_UNTRACKED:
			case GIT_DELTA_IGNORED:
				if (!FLAG_SET(opts, GIT_DIFF_FIND_FROM_UNTRACKED))
					continue;
				break;
			default:
				/* all other status values will be skipped */
				continue;
			}

			/* cap on maximum targets we'll examine (per "from" file) */
			if (++tried_targets > opts.target_limit)
				break;

			/* calculate similarity for this pair and find best match */
			if ((error = similarity_measure(
					&similarity, diff, &opts, cache, 2 * i, 2 * j + 1)) < 0)
				goto cleanup;
			if (similarity < 0) {
				--tried_targets;
				continue;
			}
			if (matches[i].similarity < (uint32_t)similarity) {
				matches[i].similarity = (uint32_t)similarity;
				matches[i].idx = j;
			}
		}
	}

	/* next rewrite the diffs with renames / copies */

	git_vector_foreach(&diff->deltas, i, from) {
		if (!matches[i].similarity)
			continue;

		to = GIT_VECTOR_GET(&diff->deltas, matches[i].idx);
		assert(to);

		similarity = (int)matches[i].similarity;

		/*
		 * Four possible outcomes here:
		 */

		/* 1. DELETED "from" with match over rename threshold becomes
		 *    RENAMED "from" record (and "to" record goes away)
		 */
		if (from->status == GIT_DELTA_DELETED) {
			if (similarity < (int)opts.rename_threshold)
				continue;

			to->flags |= GIT_DIFF_FLAG__TO_DELETE;

			from->status = GIT_DELTA_RENAMED;
			from->similarity = (uint32_t)similarity;
			memcpy(&from->new_file, &to->new_file, sizeof(to->new_file));
			validate_delta(from);

			num_rewrites++;
			continue;
		}

		/* 2. SPLIT MODIFIED "from" with match over rename threshold becomes
		 *    ADDED "from" record (with no SPLIT) and RENAMED "to" record
		 */
		if (from->status == GIT_DELTA_MODIFIED &&
			(from->flags & GIT_DIFF_FLAG__TO_SPLIT) != 0) {

			if (similarity < (int)opts.rename_threshold)
				continue;

			convert_to_rename_and_add(diff, from, to, similarity);
			num_rewrites--;
			num_updates++;
			continue;
		}

		/* 3. MODIFIED "from" with FIND_RENAMES_FROM_REWRITES with similar
		 *    "to" and self-similarity below rename_from_rewrite_threshold
		 *    becomes newly ADDED "from" and RENAMED "to".
		 */
		if (from->status == GIT_DELTA_MODIFIED &&
			FLAG_SET(opts, GIT_DIFF_FIND_RENAMES_FROM_REWRITES) &&
			similarity > (int)opts.rename_threshold)
		{
			int self_similarity;

			if ((error = similarity_measure(&self_similarity,
					diff, &opts, cache, 2 * i, 2 * i + 1)) < 0)
				goto cleanup;

			if (self_similarity >= 0 &&
				self_similarity < (int)opts.rename_from_rewrite_threshold) {

				convert_to_rename_and_add(diff, from, to, similarity);
				num_updates++;
				continue;
			}
		}

		/* 4. if "from" -> "to" over copy threshold, "to" becomes COPIED */
		if (similarity < (int)opts.copy_threshold)
			continue;

		/* convert "to" to a COPIED record */
		to->status = GIT_DELTA_COPIED;
		to->similarity = (uint32_t)similarity;
		memcpy(&to->old_file, &from->old_file, sizeof(to->old_file));
		validate_delta(to);

		validate_delta(from);

		num_updates++;
	}

	if (num_rewrites > 0)
		error = apply_splits_and_deletes(
			diff, diff->deltas.length - num_rewrites,
			FLAG_SET(opts, GIT_DIFF_BREAK_REWRITES));

	if (num_rewrites > 0 || num_updates > 0)
		git_vector_sort(&diff->deltas);

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
