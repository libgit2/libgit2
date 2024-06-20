/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "blame.h"

#include "commit.h"
#include "reader.h"
#include "tree.h"

#include "git2/blob.h"
#include "git2/revwalk.h"

int git_blame_options_init(git_blame_options *opts, unsigned int version)
{
	GIT_INIT_STRUCTURE_FROM_TEMPLATE(opts, version,
		git_blame_options, GIT_BLAME_OPTIONS_INIT);
	return 0;
}

static int normalize_options(
	git_blame_options *out,
	const git_blame_options *in)
{
	git_blame_options default_opts = GIT_BLAME_OPTIONS_INIT;

	memcpy(out, in ? in : &default_opts, sizeof(git_blame_options));

	return 0;
}

static git_blame *blame_alloc(
	git_repository *repo,
	git_blame_options *given_opts,
	const char *path)
{
	git_blame *blame;

	if ((blame = git__calloc(1, sizeof(git_blame))) == NULL)
		return NULL;

	blame->repository = repo;

	if (normalize_options(&blame->options, given_opts) < 0 ||
	    (blame->path = git__strdup(path)) == NULL) {
		git_blame_free(blame);
		return NULL;
	}

	return blame;
}

struct diff_line_data {
	git_blame *blame;
	const git_oid *commit_id;
	bool has_deltas;
	bool reassigned;
};

static int diff_line_cb(
	const git_diff_delta *delta_diff,
	const git_diff_hunk *hunk_diff,
	const git_diff_line *line_diff,
	void *payload)
{
	struct diff_line_data *diff_line_data = payload;
	git_blame *blame = diff_line_data->blame;
	const git_oid *commit_id = diff_line_data->commit_id;
	bool current_is_presumptive;
	git_blame_line *line;

	GIT_UNUSED(delta_diff);
	GIT_UNUSED(hunk_diff);

	printf("%d\n", line_diff->new_lineno);

	diff_line_data->has_deltas = true;

	/* Ignore deletions. */
	if (line_diff->new_lineno < 0)
		return 0;

	GIT_ASSERT(line_diff->new_lineno <= (int)blame->lines.size);

	printf("%c / %d / %d / %.*s", line_diff->origin, line_diff->old_lineno, line_diff->new_lineno, (int)line_diff->content_len, line_diff->content);

	/*
	 * We've already assigned presumptive blame to the current commit,
	 * so here we're only interested in context lines, which are lines
	 * that are unchanged from the parent. A context line indicates
	 * that the blame doesn't belong to the current commit, but to this
	 * parentage. We'll reassign it to this parent and then continue.
	 */
	if (line_diff->origin != GIT_DIFF_LINE_CONTEXT)
		return 0;

	line = git_array_get(blame->lines, (size_t)(line_diff->new_lineno - 1));

	printf("%c / %s\n", line->definitive ? '!' : '?', git_oid_tostr_s(&line->culprit));

	/*
	 * If the current line is already blamed, nothing to do.
	 */
	if (line->definitive)
		return 0;

	/*
	 * Make sure that we're examining a presumptive commit and not
	 * something where we've already reassigned blame.
	 */
	current_is_presumptive =
		blame->current_commit ?
		git_oid_equal(&line->culprit, git_commit_id(blame->current_commit)) :
		git_oid_is_zero(&line->culprit);

	if (current_is_presumptive) {
		git_oid_cpy(&line->culprit, commit_id);
		diff_line_data->reassigned = 1;
	}

	return 0;
}

static int setup_contents_lines(git_blame *blame)
{
	const git_oid *current_commit_id;
	const char *start, *p;
	size_t remain = blame->contents_len;
	git_blame_line *line;

	current_commit_id = blame->current_commit ?
		git_commit_id(blame->current_commit) :
		NULL;

	/*
	 * Set up the lines - we are the presumptive blame for all
	 * changes, and we will diff against our parents to reassign
	 * that presumptive blame to one of them, or take definitive
	 * ownership.
	 */
	for (start = p = blame->contents, remain = blame->contents_len;
	     remain > 0;
	     p++, remain--) {
		if (*p == '\n') {
			if ((line = git_array_alloc(blame->lines)) == NULL)
				return -1;

			line->definitive = 0;
			line->contents = start;
			line->contents_len = p - start;

			if (current_commit_id)
				git_oid_cpy(&line->culprit, current_commit_id);
			else
				git_oid_clear(&line->culprit, blame->repository->oid_type);

			start = remain ? p + 1 : NULL;
		}
	}

	/* TODO: test no trailing newline */
	if (start != p) {
		if ((line = git_array_alloc(blame->lines)) == NULL)
			return -1;

		line->contents = start;
		line->contents_len = p - start;
	}

	/*
	 * diff's line callback uses ints for line numbers
	 */
	if (blame->lines.size >= INT_MAX) {
		git_error_set(GIT_ERROR_INVALID, "file is too large to blame");
		return -1;
	}

	return 0;
}

static int setup_blame_from_buf(git_blame *blame, git_str *buf)
{
	git_reference *head;

	git_str_swap(&blame->contents_buf, buf);
	blame->contents = blame->contents_buf.ptr;
	blame->contents_len = blame->contents_buf.size;

	if (git_repository_head(&head, blame->repository) < 0 ||
	    git_reference_resolve(&blame->head_reference, head) < 0)
		return -1;

	blame->current_parents = git_reference_target(blame->head_reference);
	blame->current_parents_len = 1;

	git_reference_free(head);

	return setup_contents_lines(blame);
}

static int setup_blame_from_head(git_blame *blame)
{
	git_oid commit_id;
	git_commit *commit = NULL;
	git_tree *tree = NULL;
	git_tree_entry *tree_entry = NULL;
	git_blob *blob = NULL;
	int error = -1;

	if (git_revwalk_next(&commit_id, blame->revwalk) < 0 ||
	    git_commit_lookup(&commit, blame->repository, &commit_id) < 0 ||
	    git_commit_dup(&blame->current_commit, commit) < 0 ||
	    git_commit_tree(&tree, commit) < 0 ||
	    git_tree_entry_bypath(&tree_entry, tree, blame->path) < 0 ||
	    git_blob_lookup(&blob, blame->repository, &tree_entry->oid) < 0 ||
	    git_blob_dup(&blame->contents_blob, blob) < 0)
		goto done;

	blame->contents = git_blob_rawcontent(blame->contents_blob);
	blame->contents_len = git_blob_rawsize(blame->contents_blob);

	blame->current_parents = blame->current_commit->parent_ids.ptr;
	blame->current_parents_len = blame->current_commit->parent_ids.size;

	error = setup_contents_lines(blame);

done:
	git_blob_free(blob);
	git_tree_entry_free(tree_entry);
	git_tree_free(tree);
	git_commit_free(commit);
	return error;
}

static int compare_to_parent(
	bool *is_unchanged,
	bool *has_reassigned,
	git_blame *blame,
	const git_oid *commit_id)
{
	git_commit *commit = NULL;
	git_tree *tree = NULL;
	git_tree_entry *tree_entry = NULL;
	git_blob *blob = NULL;
	git_diff_options diff_options = GIT_DIFF_OPTIONS_INIT;
	struct diff_line_data diff_line_data;
	const char *path = blame->path;
	int error = -1;

	/* TODO: move options into blame so that we don't set them up over and over again */
	diff_options.context_lines = UINT32_MAX;

	diff_line_data.blame = blame;
	diff_line_data.commit_id = commit_id;
	diff_line_data.has_deltas = false;
	diff_line_data.reassigned = false;

	if (git_commit_lookup(&commit, blame->repository, commit_id) < 0 ||
	    git_commit_tree(&tree, commit) < 0)
		goto done;

	/* TODO: handle renames */
	if ((error = git_tree_entry_bypath(&tree_entry, tree, blame->path)) < 0) {
		if (error == GIT_ENOTFOUND)
			error = 0;

		goto done;
	}

	if ((error = git_blob_lookup(&blob, blame->repository, &tree_entry->oid)) < 0 ||
	    (error = git_diff_blob_to_buffer(blob, path,
			blame->contents, blame->contents_len,
			blame->path, &diff_options, NULL, NULL,
			NULL, diff_line_cb, &diff_line_data)) < 0)
		goto done;

	*is_unchanged = !diff_line_data.has_deltas;
	*has_reassigned = diff_line_data.reassigned;

done:
	git_blob_free(blob);
	git_tree_entry_free(tree_entry);
	git_tree_free(tree);
	git_commit_free(commit);

	return error;
}

static int pass_presumptive_ownership(
	git_blame *blame,
	const git_oid *parent_oid)
{
	const git_oid *current_commit_id;
	git_blame_line *line;
	size_t i;

	current_commit_id = blame->current_commit ?
		git_commit_id(blame->current_commit) :
		NULL;

	for (i = 0; i < blame->lines.size; i++) {
		bool match;

		line = git_array_get(blame->lines, i);

		if (line->definitive)
			continue;

		match = current_commit_id ?
			git_oid_equal(&line->culprit, current_commit_id) :
			git_oid_is_zero(&line->culprit);

		if (match)
			git_oid_cpy(&line->culprit, parent_oid);
	}

	return 0;
}

static int take_some_ownership(git_blame *blame)
{
	const git_oid *current_commit_id;
	git_blame_line *line;
	size_t i;

	current_commit_id = blame->current_commit ?
		git_commit_id(blame->current_commit) :
		NULL;

	for (i = 0; i < blame->lines.size; i++) {
		bool match;

		line = git_array_get(blame->lines, i);

		if (line->definitive)
			continue;

		match = current_commit_id ?
			git_oid_equal(&line->culprit, current_commit_id) :
			git_oid_is_zero(&line->culprit);

		if (match)
			line->definitive = 1;
	}

	return 0;
}

static void dump_state(git_blame *blame)
{
	git_blame_line *line;
	size_t i;

	for (i = 0; i < blame->lines.size; i++) {
		line = git_array_get(blame->lines, i);

		printf("%ld %c %s %.*s\n",
			i,
			line->definitive ? '!' : '?',
			git_oid_tostr_s(&line->culprit),
			(int)line->contents_len,
			line->contents);
	}
}

static int consider_current_commit(git_blame *blame)
{
	size_t i, parent_count;
	int error = -1;

	printf("CONSIDERING CURRENT COMMIT\n");

	/* TODO: honor first parent mode here? */
	parent_count = blame->current_parents_len;

	/*
	 * Compare to each parent - this will reassign presumptive blame
	 * for any lines that originated with them.
	 */
	for (i = 0; i < parent_count; i++) {
		bool is_unchanged = false;
		bool has_reassigned = false;

		printf("  EXAMINING PARENT: %d\n", (int)i);

		if (compare_to_parent(&is_unchanged,
				&has_reassigned,
				blame,
				&blame->current_parents[i]) < 0)
			goto done;

		/*
		 * If we were unchanged from this parent, then all the
		 * presumptive blame moves to them.
		 */
		if (is_unchanged) {
			printf("UNCHANGED!\n");
			error = pass_presumptive_ownership(blame, &blame->current_parents[i]);
			goto done;
		}

		/*
		 * If this commit didn't contribute to the blame,
		 * don't follow it.
		 *
		 * TODO: drop the first-parent check - it should be
		 * contributing too!
		 */
		/*
		if (!has_reassigned && i > 0) {
			printf("HIDING: %s\n", git_oid_tostr_s(&blame->current_parents[i]));
			git_revwalk_hide(blame->revwalk, &blame->current_parents[i]);
		}
		*/
	}

	/*
	 * Take definitive ownership of any lines that our parents didn't
	 * touch.
	 */

printf("TAKING SOME OWNERSHIP\n");
	error = take_some_ownership(blame);

done:
	printf("DONE ERROR IS: %d\n", error);
	return error;
}

/* TODO: coalesce with setup_from_head */
static int move_next_commit(git_blame *blame)
{
	git_oid commit_id;
	git_commit *commit = NULL;
	int error = -1;

	git_commit_free(blame->current_commit);
	blame->current_commit = NULL;

	/* TODO: lookup the blob and ignore seen blobs? */

	if (git_revwalk_next(&commit_id, blame->revwalk) < 0 ||
	    git_commit_lookup(&commit, blame->repository, &commit_id) < 0 ||
	    git_commit_dup(&blame->current_commit, commit) < 0)
		goto done;

printf("NOW: %s\n", git_oid_tostr_s(&commit_id));

	blame->current_parents = blame->current_commit->parent_ids.ptr;
	blame->current_parents_len = blame->current_commit->parent_ids.size;

	error = 0;

done:
	git_commit_free(commit);
	return error;
}

static int blame_file_from_buffer(
	git_blame **out,
	git_repository *repo,
	const char *path,
	git_str *contents_buf,
	git_blame_options *options)
{
	git_blame *blame;
	int error;

	if ((blame = blame_alloc(repo, options, path)) == NULL)
		goto on_error;

	/* TODO: commit boundaries */
	if (git_revwalk_new(&blame->revwalk, blame->repository) < 0 ||
	    git_revwalk_sorting(blame->revwalk, GIT_SORT_TOPOLOGICAL) < 0 ||
	    git_revwalk_push_head(blame->revwalk) < 0)
		goto on_error;

	error = contents_buf ?
	        setup_blame_from_buf(blame, contents_buf) :
		setup_blame_from_head(blame);

dump_state(blame);

	do {
printf("LOOP\n");
		if ((error = consider_current_commit(blame)) < 0) {
			if (error == GIT_ITEROVER) {
				printf("DONE!\n");
				break;
			}

			goto on_error;
		}

	dump_state(blame);

		if (move_next_commit(blame) < 0)
			goto on_error;
	} while (1);

printf("=========================================================\n");

	if (error != GIT_ITEROVER)
		goto on_error;

	*out = blame;
	return 0;

on_error:
	git_blame_free(blame);
	return -1;
}

int git_blame_file(
	git_blame **out,
	git_repository *repo,
	const char *path,
	git_blame_options *options)
{
	git_reader *reader = NULL;
	git_str contents = GIT_STR_INIT;
	int error = -1;

	/*
	 * TODO: need an option (like apply) to know whether we're
	 * looking at the workdir, the index, or HEAD.
	 */

	if (git_reader_for_workdir(&reader, repo, false) < 0 ||
	    git_reader_read(&contents, NULL, NULL, reader, path) < 0)
		goto done;

	error = blame_file_from_buffer(out, repo, path, &contents, options);

done:
	git_str_dispose(&contents);
	git_reader_free(reader);
	return error;
}

int git_blame_file_from_buffer(
	git_blame **out,
	git_repository *repo,
	const char *path,
	const char *contents,
	size_t contents_len,
	git_blame_options *options)
{
	git_str contents_buf = GIT_STR_INIT;
	int error = -1;

	GIT_ASSERT_ARG(out);
	GIT_ASSERT_ARG(repo);
	GIT_ASSERT_ARG(path);
	GIT_ASSERT_ARG(contents);

	if (git_str_put(&contents_buf, contents, contents_len) < 0)
		goto done;

	error = blame_file_from_buffer(out, repo, path, &contents_buf, options);

done:
	git_str_dispose(&contents_buf);
	return error;
}

int git_blame_buffer(
	git_blame **out,
	git_blame *base,
	const char *buffer,
	size_t buffer_len)
{
	git_blame *blame;

	GIT_ASSERT_ARG(out);
	GIT_ASSERT_ARG(base);
	GIT_ASSERT_ARG(buffer || !buffer_len);

	if ((blame = blame_alloc(base->repository, &base->options, base->path)) == NULL)
		return -1;

if (1)
return -42;

	*out = blame;
	return 0;
}

size_t git_blame_hunk_count(git_blame *blame)
{
	GIT_ASSERT_ARG(blame);

	return 42;
}

const git_blame_hunk *git_blame_hunk_byindex(
	git_blame *blame,
	size_t index)
{
	GIT_ASSERT_ARG_WITH_RETVAL(blame, NULL);

	return (index == 0) ? NULL : NULL;
}

const git_blame_hunk *git_blame_hunk_byline(
	git_blame *blame,
	size_t lineno)
{
	GIT_ASSERT_ARG_WITH_RETVAL(blame, NULL);

	return (lineno == 0) ? NULL : NULL;
}

#ifndef GIT_DEPRECATE_HARD
uint32_t git_blame_get_hunk_count(git_blame *blame)
{
	size_t count = git_blame_hunk_count(blame);
	GIT_ASSERT(count < UINT32_MAX);
	return (uint32_t)count;
}

const git_blame_hunk *git_blame_get_hunk_byindex(
	git_blame *blame,
	uint32_t index)
{
	return git_blame_hunk_byindex(blame, index);
}

const git_blame_hunk *git_blame_get_hunk_byline(
	git_blame *blame,
	size_t lineno)
{
	return git_blame_hunk_byline(blame, lineno);
}
#endif

void git_blame_free(git_blame *blame)
{
	if (!blame)
		return;

	git_reference_free(blame->head_reference);
	git_commit_free(blame->current_commit);
	git_revwalk_free(blame->revwalk);
	git_str_dispose(&blame->contents_buf);
	git__free(blame);
}
