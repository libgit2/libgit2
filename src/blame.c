/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "blame.h"
#include "git2/commit.h"
#include "git2/revparse.h"
#include "git2/revwalk.h"
#include "git2/tree.h"
#include "git2/diff.h"
#include "git2/blob.h"
#include "util.h"
#include "repository.h"
#include "blame_git.h"


static int hunk_search_cmp_helper(const void *key, size_t start_line, size_t num_lines)
{
	uint32_t lineno = *(size_t*)key;
	if (lineno < start_line)
		return -1;
	if (lineno >= ((uint32_t)start_line + num_lines))
		return 1;
	return 0;
}
static int hunk_byfinalline_search_cmp(const void *key, const void *entry)
{
	git_blame_hunk *hunk = (git_blame_hunk*)entry;
	return hunk_search_cmp_helper(key, hunk->final_start_line_number, hunk->lines_in_hunk);
}
static int hunk_sort_cmp_by_start_line(const void *_a, const void *_b)
{
	git_blame_hunk *a = (git_blame_hunk*)_a,
						*b = (git_blame_hunk*)_b;

	return a->final_start_line_number - b->final_start_line_number;
}

static bool hunk_ends_at_or_before_line(git_blame_hunk *hunk, size_t line)
{
	return line >= (size_t)(hunk->final_start_line_number + hunk->lines_in_hunk - 1);
}

static bool hunk_starts_at_or_after_line(git_blame_hunk *hunk, size_t line)
{
	return line <= hunk->final_start_line_number;
}

static git_blame_hunk* new_hunk(uint16_t start, uint16_t lines, uint16_t orig_start, const char *path)
{
	git_blame_hunk *hunk = git__calloc(1, sizeof(git_blame_hunk));
	if (!hunk) return NULL;

	hunk->lines_in_hunk = lines;
	hunk->final_start_line_number = start;
	hunk->orig_start_line_number = orig_start;
	hunk->orig_path = path ? git__strdup(path) : NULL;

	return hunk;
}

git_blame_hunk* git_blame__alloc_hunk()
{
	return new_hunk(0,0,0,NULL);
}

static git_blame_hunk* dup_hunk(git_blame_hunk *hunk)
{
	git_blame_hunk *newhunk = new_hunk(hunk->final_start_line_number, hunk->lines_in_hunk, hunk->orig_start_line_number, hunk->orig_path);
	git_oid_cpy(&newhunk->orig_commit_id, &hunk->orig_commit_id);
	git_oid_cpy(&newhunk->final_commit_id, &hunk->final_commit_id);
	return newhunk;
}

static void free_hunk(git_blame_hunk *hunk)
{
	git__free((void*)hunk->orig_path);
	git__free(hunk);
}

/* Starting with the hunk that includes start_line, shift all following hunks'
 * final_start_line by shift_by lines */
static void shift_hunks_by_final(git_vector *v, size_t start_line, int shift_by)
{
	size_t i;

	if (!git_vector_bsearch2( &i, v, hunk_byfinalline_search_cmp, &start_line)) {
		for (; i < v->length; i++) {
			git_blame_hunk *hunk = (git_blame_hunk*)v->contents[i];
			hunk->final_start_line_number += shift_by;
		}
	}
}
static int paths_cmp(const void *a, const void *b) { return git__strcmp((char*)a, (char*)b); }
git_blame* git_blame__alloc(
	git_repository *repo,
	git_blame_options opts,
	const char *path)
{
	git_blame *gbr = (git_blame*)git__calloc(1, sizeof(git_blame));
	if (!gbr) {
		giterr_set_oom();
		return NULL;
	}
	git_vector_init(&gbr->hunks, 8, hunk_sort_cmp_by_start_line);
	git_vector_init(&gbr->unclaimed_hunks, 8, hunk_sort_cmp_by_start_line);
	git_vector_init(&gbr->paths, 8, paths_cmp);
	gbr->repository = repo;
	gbr->options = opts;
	gbr->path = git__strdup(path);
	git_vector_insert(&gbr->paths, git__strdup(path));
	gbr->final_blob = NULL;
	return gbr;
}

void git_blame_free(git_blame *blame)
{
	size_t i;
	git_blame_hunk *hunk;
	char *path;

	if (!blame) return;

	git_vector_foreach(&blame->hunks, i, hunk)
		free_hunk(hunk);
	git_vector_free(&blame->hunks);

	git_vector_foreach(&blame->unclaimed_hunks, i, hunk)
		free_hunk(hunk);
	git_vector_free(&blame->unclaimed_hunks);

	git_vector_foreach(&blame->paths, i, path)
		git__free(path);
	git_vector_free(&blame->paths);

	git__free((void*)blame->path);
	git_blob_free(blame->final_blob);
	git__free(blame);
}

uint32_t git_blame_get_hunk_count(git_blame *blame)
{
	assert(blame);
	return blame->hunks.length;
}

const git_blame_hunk *git_blame_get_hunk_byindex(git_blame *blame, uint32_t index)
{
	assert(blame);
	return (git_blame_hunk*)git_vector_get(&blame->hunks, index);
}

const git_blame_hunk *git_blame_get_hunk_byline(git_blame *blame, uint32_t lineno)
{
	size_t i;
	assert(blame);

	if (!git_vector_bsearch2( &i, &blame->hunks, hunk_byfinalline_search_cmp, &lineno)) {
		return git_blame_get_hunk_byindex(blame, i);
	}

	return NULL;
}

static void normalize_options(
		git_blame_options *out,
		const git_blame_options *in,
		git_repository *repo)
{
	git_blame_options dummy = GIT_BLAME_OPTIONS_INIT;
	if (!in) in = &dummy;

	memcpy(out, in, sizeof(git_blame_options));

	/* No newest_commit => HEAD */
	if (git_oid_iszero(&out->newest_commit)) {
		git_reference_name_to_id(&out->newest_commit, repo, "HEAD");
	}
}

static git_blame_hunk *split_hunk_in_vector(
		git_vector *vec,
		git_blame_hunk *hunk,
		size_t rel_line,
		bool return_new)
{
	size_t new_line_count;
	git_blame_hunk *nh;

	/* Don't split if already at a boundary */
	if (rel_line <= 0 ||
	    rel_line >= hunk->lines_in_hunk)
	{
		return hunk;
	}

	new_line_count = hunk->lines_in_hunk - rel_line;
	nh = new_hunk(hunk->final_start_line_number+rel_line, new_line_count,
			hunk->orig_start_line_number+rel_line, hunk->orig_path);
	git_oid_cpy(&nh->final_commit_id, &hunk->final_commit_id);
	git_oid_cpy(&nh->orig_commit_id, &hunk->orig_commit_id);

	/* Adjust hunk that was split */
	hunk->lines_in_hunk -= new_line_count;
	git_vector_insert_sorted(vec, nh, NULL);
	{
		git_blame_hunk *ret = return_new ? nh : hunk;
		return ret;
	}
}


/*
 * To allow quick access to the contents of nth line in the
 * final image, prepare an index in the scoreboard.
 */
static int prepare_lines(struct scoreboard *sb)
{
	const char *buf = sb->final_buf;
	git_off_t len = sb->final_buf_size;
	int num = 0, incomplete = 0, bol = 1;

	if (len && buf[len-1] != '\n')
		incomplete++; /* incomplete line at the end */
	while (len--) {
		if (bol) {
			bol = 0;
		}
		if (*buf++ == '\n') {
			num++;
			bol = 1;
		}
	}
	sb->num_lines = num + incomplete;
	return sb->num_lines;
}

static git_blame_hunk* hunk_from_entry(struct blame_entry *e)
{
	git_blame_hunk *h = new_hunk(
			e->lno+1, e->num_lines, e->s_lno+1, e->suspect->path);
	git_oid_cpy(&h->final_commit_id, git_commit_id(e->suspect->commit));
	return h;
}

static int walk_and_mark(git_blame *blame)
{
	int error;

	struct scoreboard sb = {0};
	struct blame_entry *ent = NULL;
	git_blob *blob = NULL;
	struct origin *o;

	if ((error = git_commit_lookup(&sb.final, blame->repository, &blame->options.newest_commit)) < 0 ||
		 (error = git_object_lookup_bypath((git_object**)&blob, (git_object*)sb.final, blame->path, GIT_OBJ_BLOB)) < 0)
		goto cleanup;
	sb.final_buf = git_blob_rawcontent(blob);
	sb.final_buf_size = git_blob_rawsize(blob);
	if ((error = get_origin(&o, &sb, sb.final, blame->path)) < 0)
		goto cleanup;

	ent = git__calloc(1, sizeof(*ent));
	ent->num_lines = prepare_lines(&sb);
	ent->suspect = o;
	sb.ent = ent;
	sb.path = blame->path;
	sb.blame = blame;

	assign_blame(&sb, blame->options.flags);
	coalesce(&sb);

cleanup:
	for (ent = sb.ent; ent; ) {
		struct blame_entry *e = ent->next;
		struct origin *o = ent->suspect;

		git_vector_insert(&blame->hunks, hunk_from_entry(ent));

		origin_decref(o);
		git__free(ent);
		ent = e;
	}

	git_blob_free(blob);
	return error;
}

static int load_blob(git_blame *blame, git_repository *repo, git_oid *commit_id, const char *path)
{
	int retval = -1;
	git_commit *commit = NULL;
	git_tree *tree = NULL;
	git_tree_entry *tree_entry = NULL;
	git_object *obj = NULL;

	if (((retval = git_commit_lookup(&commit, repo, commit_id)) < 0) ||
		 ((retval = git_object_lookup_bypath(&obj, (git_object*)commit, path, GIT_OBJ_BLOB)) < 0) ||
	    ((retval = git_object_type(obj)) != GIT_OBJ_BLOB))
		goto cleanup;
	blame->final_blob = (git_blob*)obj;

cleanup:
	git_tree_entry_free(tree_entry);
	git_tree_free(tree);
	git_commit_free(commit);
	return retval;
}

/*******************************************************************************
 * File blaming
 ******************************************************************************/

int git_blame_file(
		git_blame **out,
		git_repository *repo,
		const char *path,
		git_blame_options *options)
{
	int error = -1;
	git_blame_options normOptions = GIT_BLAME_OPTIONS_INIT;
	git_blame *blame = NULL;

	assert(out && repo && path);
	normalize_options(&normOptions, options, repo);

	blame = git_blame__alloc(repo, normOptions, path);
	GITERR_CHECK_ALLOC(blame);

	if ((error = load_blob(blame, repo, &normOptions.newest_commit, path)) < 0)
		goto on_error;

	if ((error = walk_and_mark(blame)) < 0)
		goto on_error;

	*out = blame;
	return 0;

on_error:
	git_blame_free(blame);
	return error;
}

/*******************************************************************************
 * Buffer blaming
 *******************************************************************************/

static bool hunk_is_bufferblame(git_blame_hunk *hunk)
{
	return git_oid_iszero(&hunk->final_commit_id);
}

static int buffer_hunk_cb(
	const git_diff_delta *delta,
	const git_diff_range *range,
	const char *header,
	size_t header_len,
	void *payload)
{
	git_blame *blame = (git_blame*)payload;
	size_t wedge_line;

	GIT_UNUSED(delta);
	GIT_UNUSED(header);
	GIT_UNUSED(header_len);

	wedge_line = (range->old_lines == 0) ? range->new_start : range->old_start;
	blame->current_diff_line = wedge_line;

	/* If this hunk doesn't start between existing hunks, split a hunk up so it does */
	blame->current_hunk = (git_blame_hunk*)git_blame_get_hunk_byline(blame, wedge_line);
	if (!hunk_starts_at_or_after_line(blame->current_hunk, wedge_line)){
		blame->current_hunk = split_hunk_in_vector(&blame->hunks, blame->current_hunk,
				wedge_line - blame->current_hunk->orig_start_line_number, true);
	}

	return 0;
}

static int ptrs_equal_cmp(const void *a, const void *b) { return a<b ? -1 : a>b ? 1 : 0; }
static int buffer_line_cb(
	const git_diff_delta *delta,
	const git_diff_range *range,
	char line_origin,
	const char *content,
	size_t content_len,
	void *payload)
{
	git_blame *blame = (git_blame*)payload;

	GIT_UNUSED(delta);
	GIT_UNUSED(range);
	GIT_UNUSED(content);
	GIT_UNUSED(content_len);

#ifdef DO_DEBUG
	{
		char *str = git__substrdup(content, content_len);
		git__free(str);
	}
#endif

	if (line_origin == GIT_DIFF_LINE_ADDITION) {
		if (hunk_is_bufferblame(blame->current_hunk) &&
		    hunk_ends_at_or_before_line(blame->current_hunk, blame->current_diff_line)) {
			/* Append to the current buffer-blame hunk */
			blame->current_hunk->lines_in_hunk++;
			shift_hunks_by_final(&blame->hunks, blame->current_diff_line+1, 1);
		} else {
			/* Create a new buffer-blame hunk with this line */
			shift_hunks_by_final(&blame->hunks, blame->current_diff_line, 1);
			blame->current_hunk = new_hunk(blame->current_diff_line, 1, 0, blame->path);
			git_vector_insert_sorted(&blame->hunks, blame->current_hunk, NULL);
		}
		blame->current_diff_line++;
	}

	if (line_origin == GIT_DIFF_LINE_DELETION) {
		/* Trim the line from the current hunk; remove it if it's now empty */
		size_t shift_base = blame->current_diff_line + blame->current_hunk->lines_in_hunk+1;

		if (--(blame->current_hunk->lines_in_hunk) == 0) {
			size_t i;
			shift_base--;
			if (!git_vector_search2(&i, &blame->hunks, ptrs_equal_cmp, blame->current_hunk)) {
				git_vector_remove(&blame->hunks, i);
				free_hunk(blame->current_hunk);
				blame->current_hunk = (git_blame_hunk*)git_blame_get_hunk_byindex(blame, i);
			}
		}
		shift_hunks_by_final(&blame->hunks, shift_base, -1);
	}
	return 0;
}

int git_blame_buffer(
		git_blame **out,
		git_blame *reference,
		const char *buffer,
		size_t buffer_len)
{
	git_blame *blame;
	git_diff_options diffopts = GIT_DIFF_OPTIONS_INIT;
	size_t i;
	git_blame_hunk *hunk;

	diffopts.context_lines = 0;

	assert(out && reference && buffer && buffer_len);

	blame = git_blame__alloc(reference->repository, reference->options, reference->path);

	/* Duplicate all of the hunk structures in the reference blame */
	git_vector_foreach(&reference->hunks, i, hunk) {
		git_vector_insert(&blame->hunks, dup_hunk(hunk));
	}

	/* Diff to the reference blob */
	git_diff_blob_to_buffer(reference->final_blob, blame->path,
			buffer, buffer_len, blame->path,
			&diffopts, NULL, buffer_hunk_cb, buffer_line_cb, blame);

	*out = blame;
	return 0;
}
