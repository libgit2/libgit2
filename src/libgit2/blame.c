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
	git_commit *commit;
	bool has_changes;
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
	git_blame_line *line;

	GIT_UNUSED(delta_diff);
	GIT_UNUSED(hunk_diff);

	/* printf("%d\n", line_diff->new_lineno); */

	diff_line_data->has_changes = true;

	/* Ignore deletions. */
	if (line_diff->new_lineno < 0)
		return 0;

	GIT_ASSERT(line_diff->new_lineno <= (int)blame->lines.size);

	/* printf("%c / %d / %d / %.*s", line_diff->origin, line_diff->old_lineno, line_diff->new_lineno, (int)line_diff->content_len, line_diff->content); */

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

	/* printf("%c / %s\n", line->definitive ? '!' : '?', git_oid_tostr_s(git_commit_id(line->commit))); */

	/*
	 * If the current line is already blamed, nothing to do.
	 */
	if (line->definitive)
		return 0;

	/*
	 * Make sure that we're examining a presumptive commit and not
	 * something where we've already reassigned blame.
	 */
	if (line->commit == blame->current_commit) {
		git_commit_free(line->commit);
		git_commit_dup(&line->commit, diff_line_data->commit);

		diff_line_data->reassigned = 1;
	}

	return 0;
}

static int setup_contents_lines(git_blame *blame)
{
	const char *start, *p;
	size_t remain = blame->contents_len;
	git_blame_line *line;

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

			if (git_commit_dup(&line->commit, blame->current_commit) < 0)
				return -1;

			line->definitive = 0;
			line->contents = start;
			line->contents_len = p - start;

			start = remain ? p + 1 : NULL;
		}
	}

	/* TODO: test no trailing newline */
	if (start != p) {
		if ((line = git_array_alloc(blame->lines)) == NULL)
			return -1;

		if (git_commit_dup(&line->commit, blame->current_commit) < 0)
			return -1;

		line->definitive = 0;
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

static int mark_as_contributor(git_blame *blame, git_commit *commit)
{
	git_commit *dup = NULL;

	if (git_commit_dup(&dup, commit) < 0 ||
	    git_oidmap_set(blame->contributors, git_commit_id(dup), dup) < 0) {
		git_commit_free(dup);
		return -1;
	}

	return 0;
}

static int setup_blame_from_buf(git_blame *blame, git_str *buf)
{
	git_commit *fake_commit = NULL;
	git_reference *head = NULL, *head_resolved = NULL;
	git_oid *fake_parent;
	int error = -1;

	if (git_repository_head(&head, blame->repository) < 0 ||
	    git_reference_resolve(&head_resolved, head) < 0)
		goto done;

	fake_commit = git__calloc(1, sizeof(git_commit));
	GIT_ERROR_CHECK_ALLOC(fake_commit);

	fake_parent = git_array_alloc(fake_commit->parent_ids);
	GIT_ERROR_CHECK_ALLOC(fake_parent);
	git_oid_cpy(fake_parent, git_reference_target(head_resolved));

	git_oid_clear(&fake_commit->object.cached.oid, blame->repository->oid_type);

	fake_commit->object.cached.type = GIT_OBJECT_COMMIT;
	fake_commit->object.repo = blame->repository;

	if (git_commit_dup(&blame->current_commit, fake_commit) < 0 ||
	    mark_as_contributor(blame, fake_commit) < 0)
		goto done;

	git_str_swap(&blame->contents_buf, buf);
	blame->contents = blame->contents_buf.ptr;
	blame->contents_len = blame->contents_buf.size;

	error = setup_contents_lines(blame);

done:
	git_commit_free(fake_commit);
	git_reference_free(head_resolved);
	git_reference_free(head);
	return error;
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
	    git_blob_dup(&blame->contents_blob, blob) < 0 ||
	    mark_as_contributor(blame, commit) < 0)
		goto done;

	blame->contents = git_blob_rawcontent(blame->contents_blob);
	blame->contents_len = git_blob_rawsize(blame->contents_blob);

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
	git_commit *parent)
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
	diff_line_data.commit = parent;
	diff_line_data.has_changes = false;
	diff_line_data.reassigned = false;

	if (git_commit_tree(&tree, parent) < 0)
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

	*is_unchanged = !diff_line_data.has_changes;
	*has_reassigned = diff_line_data.reassigned;

done:
	git_blob_free(blob);
	git_tree_entry_free(tree_entry);
	git_tree_free(tree);
	git_commit_free(commit);

	return error;
}

static int pass_presumptive_blame(git_blame *blame, git_commit *parent)
{
	git_blame_line *line;
	size_t i;

	for (i = 0; i < blame->lines.size; i++) {
		line = git_array_get(blame->lines, i);

		if (line->definitive)
			continue;

		if (line->commit == blame->current_commit) {
			git_commit_free(line->commit);
			git_commit_dup(&line->commit, parent);
		}
	}

	return 0;
}

static int take_definitive_blame(git_blame *blame)
{
	git_blame_line *line;
	size_t i;

	for (i = 0; i < blame->lines.size; i++) {
		line = git_array_get(blame->lines, i);

		if (line->commit == blame->current_commit) {
			GIT_ASSERT(!line->definitive);
			line->definitive = 1;
		}
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
			git_oid_tostr_s(git_commit_id(line->commit)),
			(int)line->contents_len,
			line->contents);
	}
}

static int consider_current_commit(git_blame *blame)
{
	git_commit *this = NULL, *parent = NULL;
	size_t i, parent_count;
	int error = -1;

	if (git_oidmap_get_and_delete((void **)&this, blame->contributors, git_commit_id(blame->current_commit)) == GIT_ENOTFOUND) {
		return 0;
	}

	/* printf("CONSIDERING CURRENT COMMIT\n"); */

	/* TODO: honor first parent mode here? */
	parent_count = git_commit_parentcount(blame->current_commit);

	/*
	 * Compare to each parent - this will reassign presumptive blame
	 * for any lines that originated with them.
	 */
	for (i = 0; i < parent_count; i++) {
		bool is_unchanged = false;
		bool has_reassigned = false;

		/* printf("  EXAMINING PARENT: %d\n", (int)i); */

		if (git_commit_parent(&parent, blame->current_commit, i) < 0 ||
		    compare_to_parent(&is_unchanged, &has_reassigned,
				blame, parent) < 0)
			goto done;

		/*
		 * If we were unchanged from this parent, then all the
		 * presumptive blame moves to them.
		 */
		if (is_unchanged) {
		/*	printf("UNCHANGED!\n"); */

			error = pass_presumptive_blame(blame, parent);
			goto done;
		}

		/* Record this commit if it contributed. */
		if (has_reassigned)
			mark_as_contributor(blame, parent);

		git_commit_free(parent);
		parent = NULL;

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

/* printf("TAKING SOME OWNERSHIP\n");*/
	error = take_definitive_blame(blame);

done:
/*	printf("DONE ERROR IS: %d\n", error);*/
	git_commit_free(parent);
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
	int error = -1;

	if ((blame = blame_alloc(repo, options, path)) == NULL)
		goto on_error;

	/* TODO: commit boundaries */
	if (git_oidmap_new(&blame->contributors) < 0 ||
	    git_revwalk_new(&blame->revwalk, blame->repository) < 0 ||
	    git_revwalk_sorting(blame->revwalk, GIT_SORT_TOPOLOGICAL) < 0 ||
	    git_revwalk_push_head(blame->revwalk) < 0)
		goto on_error;

	error = contents_buf ?
	        setup_blame_from_buf(blame, contents_buf) :
		setup_blame_from_head(blame);

	do {
		if ((error = consider_current_commit(blame)) < 0) {
			if (error == GIT_ITEROVER) {
				printf("DONE!\n");
				break;
			}

			goto on_error;
		}

		if (move_next_commit(blame) < 0)
			goto on_error;
	} while (git_oidmap_size(blame->contributors) > 0);

/* printf("=========================================================\n"); */

dump_state(blame);

	if (error != GIT_ITEROVER)
		goto on_error;

	*out = blame;
	return 0;

on_error:
	git_blame_free(blame);
	return error;
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
	git_commit *commit;

	if (!blame)
		return;

	git_oidmap_foreach_value(blame->contributors, commit, {
		git_commit_free(commit);
	});

	git_oidmap_free(blame->contributors);
	git_commit_free(blame->current_commit);
	git_revwalk_free(blame->revwalk);
	git_str_dispose(&blame->contents_buf);
	git__free(blame);
}
