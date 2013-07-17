/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include <assert.h>

#include "checkout.h"

#include "vector.h"
#include "index.h"
#include "merge_file.h"
#include "git2/repository.h"
#include "git2/types.h"
#include "git2/index.h"
#include "git2/sys/index.h"

typedef struct {
	const git_index_entry *ancestor;
	const git_index_entry *ours;
	const git_index_entry *theirs;

	int name_collision:1,
		directoryfile:1;
} checkout_conflictdata;

GIT_INLINE(int) checkout_idxentry_cmp(
	const git_index_entry *a,
	const git_index_entry *b)
{
	if (!a && !b)
		return 0;
	else if (!a && b)
		return -1;
	else if(a && !b)
		return 1;
	else
		return strcmp(a->path, b->path);
}

static int checkout_conflictdata_cmp(const void *a, const void *b)
{
	const checkout_conflictdata *ca = a;
	const checkout_conflictdata *cb = b;
	int diff;

	if ((diff = checkout_idxentry_cmp(ca->ancestor, cb->ancestor)) == 0 &&
		(diff = checkout_idxentry_cmp(ca->ours, cb->theirs)) == 0)
		diff = checkout_idxentry_cmp(ca->theirs, cb->theirs);

	return diff;
}

int checkout_conflictdata_empty(const git_vector *conflicts, size_t idx)
{
	const checkout_conflictdata *conflict;

	if ((conflict = git_vector_get(conflicts, idx)) == NULL)
		return -1;

	return (conflict->ancestor == NULL &&
		conflict->ours == NULL &&
		conflict->theirs == NULL);
}

static int checkout_conflicts_load(checkout_data *data, git_vector *conflicts)
{
	git_index_conflict_iterator *iterator = NULL;
	const git_index_entry *ancestor, *ours, *theirs;
	checkout_conflictdata *conflict;
	int error = 0;

	if ((error = git_index_conflict_iterator_new(&iterator, data->index)) < 0)
		goto done;

	conflicts->_cmp = checkout_conflictdata_cmp;

	/* Collect the conflicts */
	while ((error = git_index_conflict_next(
		&ancestor, &ours, &theirs, iterator)) == 0) {

		conflict = git__calloc(1, sizeof(checkout_conflictdata));
		GITERR_CHECK_ALLOC(conflict);

		conflict->ancestor = ancestor;
		conflict->ours = ours;
		conflict->theirs = theirs;

		git_vector_insert(conflicts, conflict);
	}

	if (error == GIT_ITEROVER)
		error = 0;

done:
	git_index_conflict_iterator_free(iterator);

	return error;
}

GIT_INLINE(int) checkout_conflicts_cmp_entry(
	const char *path,
	const git_index_entry *entry)
{
	/* TODO: is strcmp right here?  should we use index->strcomp ? */
	return strcmp((const char *)path, entry->path);
}

static int checkout_conflicts_cmp_ancestor(const void *p, const void *c)
{
	const char *path = p;
	const checkout_conflictdata *conflict = c;

	if (!conflict->ancestor)
		return 1;

	return checkout_conflicts_cmp_entry(path, conflict->ancestor);
}

static checkout_conflictdata *checkout_conflicts_search_ancestor(
	git_vector *conflicts,
	const char *path)
{
	size_t pos;

	if (git_vector_bsearch2(&pos, conflicts, checkout_conflicts_cmp_ancestor, path) < 0)
		return NULL;

	return git_vector_get(conflicts, pos);
}

static checkout_conflictdata *checkout_conflicts_search_branch(
	git_vector *conflicts,
	const char *path)
{
	checkout_conflictdata *conflict;
	size_t i;

	git_vector_foreach(conflicts, i, conflict) {
		int cmp = -1;

		if (conflict->ancestor)
			break;

		if (conflict->ours)
			cmp = checkout_conflicts_cmp_entry(path, conflict->ours);
		else if (conflict->theirs)
			cmp = checkout_conflicts_cmp_entry(path, conflict->theirs);

		if (cmp == 0)
			return conflict;
	}

	return NULL;
}

static int checkout_conflicts_load_byname_entry(
	checkout_conflictdata **ancestor_out,
	checkout_conflictdata **ours_out,
	checkout_conflictdata **theirs_out,
	git_vector *conflicts,
	const git_index_name_entry *name_entry)
{
	checkout_conflictdata *ancestor, *ours = NULL, *theirs = NULL;
	int error = 0;

	*ancestor_out = NULL;
	*ours_out = NULL;
	*theirs_out = NULL;

	if (!name_entry->ancestor) {
		giterr_set(GITERR_INDEX, "A NAME entry exists without an ancestor");
		error = -1;
		goto done;
	}

	if (!name_entry->ours && !name_entry->theirs) {
		giterr_set(GITERR_INDEX, "A NAME entry exists without an ours or theirs");
		error = -1;
		goto done;
	}

	if ((ancestor = checkout_conflicts_search_ancestor(conflicts,
		name_entry->ancestor)) == NULL) {
		giterr_set(GITERR_INDEX,
			"A NAME entry referenced ancestor entry '%s' which does not exist in the main index",
			name_entry->ancestor);
		error = -1;
		goto done;
	}

	if (name_entry->ours) {
		if (strcmp(name_entry->ancestor, name_entry->ours) == 0)
			ours = ancestor;
		else if ((ours = checkout_conflicts_search_branch(conflicts, name_entry->ours)) == NULL ||
			ours->ours == NULL) {
			giterr_set(GITERR_INDEX,
				"A NAME entry referenced our entry '%s' which does not exist in the main index",
				name_entry->ours);
			error = -1;
			goto done;
		}
	}

	if (name_entry->theirs) {
		if (strcmp(name_entry->ancestor, name_entry->theirs) == 0)
			theirs = ancestor;
		else if ((theirs = checkout_conflicts_search_branch(conflicts, name_entry->theirs)) == NULL ||
			theirs->theirs == NULL) {
			giterr_set(GITERR_INDEX,
				"A NAME entry referenced their entry '%s' which does not exist in the main index",
				name_entry->theirs);
			error = -1;
			goto done;
		}
	}

	*ancestor_out = ancestor;
	*ours_out = ours;
	*theirs_out = theirs;

done:
	return error;
}

static int checkout_conflicts_coalesce_renames(
	checkout_data *data,
	git_vector *conflicts)
{
	const git_index_name_entry *name_entry;
	checkout_conflictdata *ancestor_conflict, *our_conflict, *their_conflict;
	size_t i, names;
	int error = 0;

	/* Juggle entries based on renames */
	for (i = 0, names = git_index_name_entrycount(data->index);
		i < names;
		i++) {

		name_entry = git_index_name_get_byindex(data->index, i);

		if ((error = checkout_conflicts_load_byname_entry(
			&ancestor_conflict, &our_conflict, &their_conflict,
			conflicts, name_entry)) < 0)
			goto done;

		if (our_conflict && our_conflict != ancestor_conflict) {
			ancestor_conflict->ours = our_conflict->ours;
			our_conflict->ours = NULL;

			if (our_conflict->theirs)
				our_conflict->name_collision = 1;

			if (our_conflict->name_collision)
				ancestor_conflict->name_collision = 1;
		}

		if (their_conflict && their_conflict != ancestor_conflict) {
			ancestor_conflict->theirs = their_conflict->theirs;
			their_conflict->theirs = NULL;

			if (their_conflict->ours)
				their_conflict->name_collision = 1;

			if (their_conflict->name_collision)
				ancestor_conflict->name_collision = 1;
		}
	}

	git_vector_remove_matching(conflicts, checkout_conflictdata_empty);

done:
	return error;
}

/* TODO: does this exist elsewhere? */
GIT_INLINE(void) path_equal_or_prefixed(
	bool *path_eq,
	bool *path_prefixed,
	const char *parent,
	const char *child)
{
	size_t child_len = strlen(child);
	size_t parent_len = strlen(parent);

	if (child_len == parent_len) {
		*path_eq = (strcmp(parent, child) == 0);
		*path_prefixed = 0;
		return;
	}
	
	*path_eq = 0;

	if (child_len < parent_len ||
		strncmp(parent, child, parent_len) != 0)
		*path_prefixed = 0;
	else
		*path_prefixed = (child[parent_len] == '/');
}

static int checkout_conflicts_mark_directoryfile(
	checkout_data *data,
	git_vector *conflicts)
{
	checkout_conflictdata *conflict;
	const git_index_entry *entry;
	size_t i, j, len;
	const char *path;
	bool eq, prefixed;
	int error = 0;

	len = git_index_entrycount(data->index);

	/* Find d/f conflicts */
	git_vector_foreach(conflicts, i, conflict) {
		if ((conflict->ours && conflict->theirs) ||
			(!conflict->ours && !conflict->theirs))
			continue;

		path = conflict->ours ?
			conflict->ours->path : conflict->theirs->path;

		if ((error = git_index_find(&j, data->index, path)) < 0) {
			if (error == GIT_ENOTFOUND)
				giterr_set(GITERR_MERGE,
					"Index inconsistency, could not find entry for expected conflict '%s'", path);

			goto done;
		}

		for (; j < len; j++) {
			if ((entry = git_index_get_byindex(data->index, j)) == NULL) {
				giterr_set(GITERR_MERGE,
					"Index inconsistency, truncated index while loading expected conflict '%s'", path);
				error = -1;
				goto done;
			}

			path_equal_or_prefixed(&eq, &prefixed, path, entry->path);

			if (eq)
				continue;

			if (prefixed)
				conflict->directoryfile = 1;

			break;
		}
	}

done:
	return error;
}

static int conflict_entry_name(
	git_buf *out,
	const char *side_name,
	const char *filename)
{
	if (git_buf_puts(out, side_name) < 0 ||
		git_buf_putc(out, ':') < 0 ||
		git_buf_puts(out, filename) < 0)
		return -1;

	return 0;
}

static int conflict_path_suffixed(
	git_buf *out,
	const char *path,
	const char *side_name)
{
	if (git_buf_puts(out, path) < 0 ||
		git_buf_putc(out, '~') < 0 ||
		git_buf_puts(out, side_name) < 0)
		return -1;

	return 0;
}

static int checkout_write_entry(
	checkout_data *data,
	checkout_conflictdata *conflict,
	const git_index_entry *side)
{
	const char *hint_path = NULL, *side_label;
	struct stat st;

	assert (side == conflict->ours ||
		side == conflict->theirs);

	git_buf_truncate(&data->path, data->workdir_len);
	if (git_buf_puts(&data->path, side->path) < 0)
		return -1;

	if (conflict->name_collision || conflict->directoryfile) {
		if (side == conflict->ours)
			side_label = data->opts.our_label ? data->opts.our_label :
				"ours";
		else if (side == conflict->theirs)
			side_label = data->opts.their_label ? data->opts.their_label :
				"theirs";

		if (git_buf_putc(&data->path, '~') < 0 ||
			git_buf_puts(&data->path, side_label) < 0)
			return -1;

		hint_path = side->path;
	}

	return git_checkout__write_content(data,
		&side->oid, git_buf_cstr(&data->path), hint_path, side->mode, &st);
}

static int checkout_write_entries(
	checkout_data *data,
	checkout_conflictdata *conflict)
{
	int error = 0;

	if ((error = checkout_write_entry(data, conflict, conflict->ours)) >= 0)
		error = checkout_write_entry(data, conflict, conflict->theirs);

	return error;
}

static int checkout_write_merge(
	checkout_data *data,
	checkout_conflictdata *conflict)
{
	git_buf our_label = GIT_BUF_INIT, their_label = GIT_BUF_INIT,
		path_suffixed = GIT_BUF_INIT, path_workdir = GIT_BUF_INIT;
	git_merge_file_input ancestor = GIT_MERGE_FILE_INPUT_INIT,
		ours = GIT_MERGE_FILE_INPUT_INIT,
		theirs = GIT_MERGE_FILE_INPUT_INIT;
	git_merge_file_result result = GIT_MERGE_FILE_RESULT_INIT;
	git_filebuf output = GIT_FILEBUF_INIT;
	const char *our_label_raw, *their_label_raw, *path;
	int error = 0;

	if ((conflict->ancestor &&
		(error = git_merge_file_input_from_index_entry(
		&ancestor, data->repo, conflict->ancestor)) < 0) ||
		(error = git_merge_file_input_from_index_entry(
		&ours, data->repo, conflict->ours)) < 0 ||
		(error = git_merge_file_input_from_index_entry(
		&theirs, data->repo, conflict->theirs)) < 0)
		goto done;

	ancestor.label = NULL;
	ours.label = our_label_raw = data->opts.our_label ? data->opts.our_label : "ours";
	theirs.label = their_label_raw = data->opts.their_label ? data->opts.their_label : "theirs";

	/* If all the paths are identical, decorate the diff3 file with the branch
	 * names.  Otherwise, append branch_name:path.
	 */
	if (conflict->ours && conflict->theirs &&
		strcmp(conflict->ours->path, conflict->theirs->path) != 0) {

		if ((error = conflict_entry_name(
			&our_label, ours.label, conflict->ours->path)) < 0 ||
			(error = conflict_entry_name(
			&their_label, theirs.label, conflict->theirs->path)) < 0)
			goto done;

		ours.label = git_buf_cstr(&our_label);
		theirs.label = git_buf_cstr(&their_label);
	}

	if ((error = git_merge_files(&result, &ancestor, &ours, &theirs, 0)) < 0)
		goto done;

	if (result.path == NULL || result.mode == 0) {
		giterr_set(GITERR_CHECKOUT, "Could not merge contents of file");
		error = GIT_EMERGECONFLICT;
		goto done;
	}

	/* Rename 2->1 conflicts need the branch name appended */
	if (conflict->name_collision) {
		/* TODO: strcmp? */
		if ((error = conflict_path_suffixed(&path_suffixed, result.path,
			(strcmp(result.path, conflict->ours->path) == 0 ?
			our_label_raw : their_label_raw))) < 0)
			goto done;
		
		path = git_buf_cstr(&path_suffixed);
	} else
		path = result.path;

	if ((error = git_buf_joinpath(&path_workdir, git_repository_workdir(data->repo), path)) < 0 ||
		(error = git_futils_mkpath2file(path_workdir.ptr, 0755) < 0) ||
		(error = git_filebuf_open(&output, path_workdir.ptr, GIT_FILEBUF_DO_NOT_BUFFER)) < 0 ||
		(error = git_filebuf_write(&output, result.data, result.len)) < 0 ||
		(error = git_filebuf_commit(&output, result.mode)) < 0)
		goto done;

done:
	git_buf_free(&our_label);
	git_buf_free(&their_label);

	git_merge_file_input_free(&ancestor);
	git_merge_file_input_free(&ours);
	git_merge_file_input_free(&theirs);
	git_merge_file_result_free(&result);
	git_buf_free(&path_workdir);
	git_buf_free(&path_suffixed);

	return error;
}

GIT_INLINE(bool) conflict_is_1_to_2(checkout_conflictdata *conflict)
{
	/* TODO: can't we detect these when we coalesce? */
	return conflict->ancestor && conflict->ours && conflict->theirs &&
		(strcmp(conflict->ancestor->path, conflict->ours->path) != 0 &&
		strcmp(conflict->ancestor->path, conflict->theirs->path) != 0 &&
		strcmp(conflict->ours->path, conflict->theirs->path) != 0);
}

int git_checkout__conflicts(checkout_data *data)
{
	git_vector conflicts = GIT_VECTOR_INIT;
	checkout_conflictdata *conflict;
	size_t i;
	int error = 0;

	if (data->strategy & GIT_CHECKOUT_SKIP_UNMERGED)
		return 0;

	if ((error = checkout_conflicts_load(data, &conflicts)) < 0 ||
		(error = checkout_conflicts_coalesce_renames(data, &conflicts)) < 0 ||
		(error = checkout_conflicts_mark_directoryfile(data, &conflicts)) < 0)
		goto done;

	git_vector_foreach(&conflicts, i, conflict) {
		/* Both deleted: nothing to do */
		if (conflict->ours == NULL && conflict->theirs == NULL)
			error = 0;

		else if ((data->strategy & GIT_CHECKOUT_USE_OURS) &&
			conflict->ours)
			error = checkout_write_entry(data, conflict, conflict->ours);
		else if ((data->strategy & GIT_CHECKOUT_USE_THEIRS) &&
			conflict->theirs)
			error = checkout_write_entry(data, conflict, conflict->theirs);

		/* Ignore the other side of name collisions. */
		else if ((data->strategy & GIT_CHECKOUT_USE_OURS) &&
			!conflict->ours && conflict->name_collision)
			error = 0;
		else if ((data->strategy & GIT_CHECKOUT_USE_THEIRS) &&
			!conflict->theirs && conflict->name_collision)
			error = 0;

		/* For modify/delete, name collisions and d/f conflicts, write
		 * the file (potentially with the name mangled.
		 */
		else if (conflict->ours != NULL && conflict->theirs == NULL)
			error = checkout_write_entry(data, conflict, conflict->ours);
		else if (conflict->ours == NULL && conflict->theirs != NULL)
			error = checkout_write_entry(data, conflict, conflict->theirs);

		/* Add/add conflicts and rename 1->2 conflicts, write the
		 * ours/theirs sides (potentially name mangled).
		 */
		else if (conflict_is_1_to_2(conflict))
			error = checkout_write_entries(data, conflict);

		/* If all sides are links, write the ours side */
		else if (S_ISLNK(conflict->ours->mode) &&
			S_ISLNK(conflict->theirs->mode))
			error = checkout_write_entry(data, conflict, conflict->ours);
		/* Link/file conflicts, write the file side */
		else if (S_ISLNK(conflict->ours->mode))
			error = checkout_write_entry(data, conflict, conflict->theirs);
		else if (S_ISLNK(conflict->theirs->mode))
			error = checkout_write_entry(data, conflict, conflict->ours);

		else
			error = checkout_write_merge(data, conflict);
	}

done:
	git_vector_foreach(&conflicts, i, conflict)
		git__free(conflict);

	git_vector_free(&conflicts);

	return error;
}
