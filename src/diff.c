/*
 * Copyright (C) 2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#include "common.h"
#include "git2/diff.h"
#include "diff.h"
#include "fileops.h"

static void diff_delta__free(git_diff_delta *delta)
{
	if (!delta)
		return;

	if (delta->new.flags & GIT_DIFF_FILE_FREE_PATH) {
		git__free((char *)delta->new.path);
		delta->new.path = NULL;
	}

	if (delta->old.flags & GIT_DIFF_FILE_FREE_PATH) {
		git__free((char *)delta->old.path);
		delta->old.path = NULL;
	}

	git__free(delta);
}

static git_diff_delta *diff_delta__alloc(
	git_diff_list *diff,
	git_status_t status,
	const char *path)
{
	git_diff_delta *delta = git__calloc(1, sizeof(git_diff_delta));
	if (!delta)
		return NULL;

	delta->old.path = git__strdup(path);
	if (delta->old.path == NULL) {
		git__free(delta);
		return NULL;
	}
	delta->old.flags |= GIT_DIFF_FILE_FREE_PATH;
	delta->new.path = delta->old.path;

	if (diff->opts.flags & GIT_DIFF_REVERSE) {
		switch (status) {
		case GIT_STATUS_ADDED:   status = GIT_STATUS_DELETED; break;
		case GIT_STATUS_DELETED: status = GIT_STATUS_ADDED; break;
		default: break; /* leave other status values alone */
		}
	}
	delta->status = status;

	return delta;
}

static git_diff_delta *diff_delta__dup(const git_diff_delta *d)
{
	git_diff_delta *delta = git__malloc(sizeof(git_diff_delta));
	if (!delta)
		return NULL;

	memcpy(delta, d, sizeof(git_diff_delta));

	delta->old.path = git__strdup(d->old.path);
	if (delta->old.path == NULL) {
		git__free(delta);
		return NULL;
	}
	delta->old.flags |= GIT_DIFF_FILE_FREE_PATH;

	if (d->new.path != d->old.path) {
		delta->new.path = git__strdup(d->new.path);
		if (delta->new.path == NULL) {
			git__free(delta->old.path);
			git__free(delta);
			return NULL;
		}
		delta->new.flags |= GIT_DIFF_FILE_FREE_PATH;
	} else {
		delta->new.path = delta->old.path;
		delta->new.flags &= ~GIT_DIFF_FILE_FREE_PATH;
	}

	return delta;
}

static git_diff_delta *diff_delta__merge_like_cgit(
	const git_diff_delta *a, const git_diff_delta *b)
{
	git_diff_delta *dup = diff_delta__dup(a);
	if (!dup)
		return NULL;

	if (git_oid_cmp(&dup->new.oid, &b->new.oid) == 0)
		return dup;

	git_oid_cpy(&dup->new.oid, &b->new.oid);

	dup->new.mode = b->new.mode;
	dup->new.size = b->new.size;
	dup->new.flags =
		(dup->new.flags & GIT_DIFF_FILE_FREE_PATH) |
		(b->new.flags & ~GIT_DIFF_FILE_FREE_PATH);

	/* Emulate C git for merging two diffs (a la 'git diff <sha>').
	 *
	 * When C git does a diff between the work dir and a tree, it actually
	 * diffs with the index but uses the workdir contents.  This emulates
	 * those choices so we can emulate the type of diff.
	 */
	if (git_oid_cmp(&dup->old.oid, &dup->new.oid) == 0) {
		if (dup->status == GIT_STATUS_DELETED)
			/* preserve pending delete info */;
		else if (b->status == GIT_STATUS_UNTRACKED ||
				 b->status == GIT_STATUS_IGNORED)
			dup->status = b->status;
		else
			dup->status = GIT_STATUS_UNMODIFIED;
	}
	else if (dup->status == GIT_STATUS_UNMODIFIED ||
			 b->status == GIT_STATUS_DELETED)
		dup->status = b->status;

	return dup;
}

static int diff_delta__from_one(
	git_diff_list *diff,
	git_status_t   status,
	const git_index_entry *entry)
{
	int error;
	git_diff_delta *delta = diff_delta__alloc(diff, status, entry->path);
	if (!delta)
		return git__rethrow(GIT_ENOMEM, "Could not allocate diff record");

	/* This fn is just for single-sided diffs */
	assert(status != GIT_STATUS_MODIFIED);

	if (delta->status == GIT_STATUS_DELETED) {
		delta->old.mode = entry->mode;
		delta->old.size = entry->file_size;
		git_oid_cpy(&delta->old.oid, &entry->oid);
	} else /* ADDED, IGNORED, UNTRACKED */ {
		delta->new.mode = entry->mode;
		delta->new.size = entry->file_size;
		git_oid_cpy(&delta->new.oid, &entry->oid);
	}

	delta->old.flags |= GIT_DIFF_FILE_VALID_OID;
	delta->new.flags |= GIT_DIFF_FILE_VALID_OID;

	if ((error = git_vector_insert(&diff->deltas, delta)) < GIT_SUCCESS)
		diff_delta__free(delta);

	return error;
}

static int diff_delta__from_two(
	git_diff_list *diff,
	git_status_t   status,
	const git_index_entry *old,
	const git_index_entry *new,
	git_oid *new_oid)
{
	int error;
	git_diff_delta *delta;

	if ((diff->opts.flags & GIT_DIFF_REVERSE) != 0) {
		const git_index_entry *temp = old;
		old = new;
		new = temp;
	}

	delta = diff_delta__alloc(diff, status, old->path);
	if (!delta)
		return git__rethrow(GIT_ENOMEM, "Could not allocate diff record");

	delta->old.mode = old->mode;
	git_oid_cpy(&delta->old.oid, &old->oid);
	delta->old.flags |= GIT_DIFF_FILE_VALID_OID;

	delta->new.mode = new->mode;
	git_oid_cpy(&delta->new.oid, new_oid ? new_oid : &new->oid);
	if (new_oid || !git_oid_iszero(&new->oid))
		delta->new.flags |= GIT_DIFF_FILE_VALID_OID;

	if ((error = git_vector_insert(&diff->deltas, delta)) < GIT_SUCCESS)
		diff_delta__free(delta);

	return error;
}

#define DIFF_SRC_PREFIX_DEFAULT "a/"
#define DIFF_DST_PREFIX_DEFAULT "b/"

static char *diff_strdup_prefix(const char *prefix)
{
	size_t len = strlen(prefix);
	char *str = git__malloc(len + 2);
	if (str != NULL) {
		memcpy(str, prefix, len + 1);
		/* append '/' at end if needed */
		if (len > 0 && str[len - 1] != '/') {
			str[len] = '/';
			str[len + 1] = '\0';
		}
	}
	return str;
}

static int diff_delta__cmp(const void *a, const void *b)
{
	const git_diff_delta *da = a, *db = b;
	int val = strcmp(da->old.path, db->old.path);
	return val ? val : ((int)da->status - (int)db->status);
}

static git_diff_list *git_diff_list_alloc(
	git_repository *repo, const git_diff_options *opts)
{
	git_diff_list *diff = git__calloc(1, sizeof(git_diff_list));
	if (diff == NULL)
		return NULL;

	diff->repo = repo;

	if (opts == NULL)
		return diff;

	memcpy(&diff->opts, opts, sizeof(git_diff_options));

	diff->opts.src_prefix = diff_strdup_prefix(
		opts->src_prefix ? opts->src_prefix : DIFF_SRC_PREFIX_DEFAULT);
	diff->opts.dst_prefix = diff_strdup_prefix(
		opts->dst_prefix ? opts->dst_prefix : DIFF_DST_PREFIX_DEFAULT);

	if (!diff->opts.src_prefix || !diff->opts.dst_prefix) {
		git__free(diff);
		return NULL;
	}

	if (diff->opts.flags & GIT_DIFF_REVERSE) {
		char *swap = diff->opts.src_prefix;
		diff->opts.src_prefix = diff->opts.dst_prefix;
		diff->opts.dst_prefix = swap;
	}

	if (git_vector_init(&diff->deltas, 0, diff_delta__cmp) < GIT_SUCCESS) {
		git__free(diff->opts.src_prefix);
		git__free(diff->opts.dst_prefix);
		git__free(diff);
		return NULL;
	}

	/* do something safe with the pathspec strarray */

	return diff;
}

void git_diff_list_free(git_diff_list *diff)
{
	git_diff_delta *delta;
	unsigned int i;

	if (!diff)
		return;

	git_vector_foreach(&diff->deltas, i, delta) {
		diff_delta__free(delta);
		diff->deltas.contents[i] = NULL;
	}
	git_vector_free(&diff->deltas);
	git__free(diff->opts.src_prefix);
	git__free(diff->opts.dst_prefix);
	git__free(diff);
}

static int oid_for_workdir_item(
	git_repository *repo,
	const git_index_entry *item,
	git_oid *oid)
{
	int error = GIT_SUCCESS;
	git_buf full_path = GIT_BUF_INIT;

	error = git_buf_joinpath(
		&full_path, git_repository_workdir(repo), item->path);
	if (error != GIT_SUCCESS)
		return error;

	/* otherwise calculate OID for file */
	if (S_ISLNK(item->mode))
		error = git_odb__hashlink(oid, full_path.ptr);
	else if (!git__is_sizet(item->file_size))
		error = git__throw(GIT_ERROR, "File size overflow for 32-bit systems");
	else {
		int fd;

		if ((fd = p_open(full_path.ptr, O_RDONLY)) < 0)
			error = git__throw(
				GIT_EOSERR, "Could not open '%s'", item->path);
		else {
			error = git_odb__hashfd(
				oid, fd, (size_t)item->file_size, GIT_OBJ_BLOB);
			p_close(fd);
		}
	}

	git_buf_free(&full_path);

	return error;
}

static int maybe_modified(
	git_iterator *old,
	const git_index_entry *oitem,
	git_iterator *new,
	const git_index_entry *nitem,
	git_diff_list *diff)
{
	int error = GIT_SUCCESS;
	git_oid noid, *use_noid = NULL;

	GIT_UNUSED(old);

	/* support "assume unchanged" & "skip worktree" bits */
	if ((oitem->flags_extended & GIT_IDXENTRY_INTENT_TO_ADD) != 0 ||
		(oitem->flags_extended & GIT_IDXENTRY_SKIP_WORKTREE) != 0)
		return GIT_SUCCESS;

	if (GIT_MODE_TYPE(oitem->mode) != GIT_MODE_TYPE(nitem->mode)) {
		error = diff_delta__from_one(diff, GIT_STATUS_DELETED, oitem);
		if (error == GIT_SUCCESS)
			error = diff_delta__from_one(diff, GIT_STATUS_ADDED, nitem);
		return error;
	}

	if (git_oid_cmp(&oitem->oid, &nitem->oid) == 0 &&
		oitem->mode == nitem->mode)
		return GIT_SUCCESS;

	if (git_oid_iszero(&nitem->oid) && new->type == GIT_ITERATOR_WORKDIR) {
		/* if they files look exactly alike, then we'll assume the same */
		if (oitem->file_size == nitem->file_size &&
			oitem->ctime.seconds == nitem->ctime.seconds &&
			oitem->mtime.seconds == nitem->mtime.seconds &&
			oitem->dev == nitem->dev &&
			oitem->ino == nitem->ino &&
			oitem->uid == nitem->uid &&
			oitem->gid == nitem->gid)
			return GIT_SUCCESS;

		/* TODO: check git attributes so we will not have to read the file
		 * in if it is marked binary.
		 */
		error = oid_for_workdir_item(diff->repo, nitem, &noid);
		if (error != GIT_SUCCESS)
			return error;

		if (git_oid_cmp(&oitem->oid, &noid) == 0 &&
			oitem->mode == nitem->mode)
			return GIT_SUCCESS;

		/* store calculated oid so we don't have to recalc later */
		use_noid = &noid;
	}

	return diff_delta__from_two(
		diff, GIT_STATUS_MODIFIED, oitem, nitem, use_noid);
}

static int diff_from_iterators(
	git_repository *repo,
	const git_diff_options *opts, /**< can be NULL for defaults */
	git_iterator *old,
	git_iterator *new,
	git_diff_list **diff_ptr)
{
	int error;
	const git_index_entry *oitem, *nitem;
	char *ignore_prefix = NULL;
	git_diff_list *diff = git_diff_list_alloc(repo, opts);
	if (!diff) {
		error = GIT_ENOMEM;
		goto cleanup;
	}

	diff->old_src = old->type;
	diff->new_src = new->type;

	if ((error = git_iterator_current(old, &oitem)) < GIT_SUCCESS ||
		(error = git_iterator_current(new, &nitem)) < GIT_SUCCESS)
		goto cleanup;

	/* run iterators building diffs */
	while (!error && (oitem || nitem)) {

		/* create DELETED records for old items not matched in new */
		if (oitem && (!nitem || strcmp(oitem->path, nitem->path) < 0)) {
			error = diff_delta__from_one(diff, GIT_STATUS_DELETED, oitem);
			if (error == GIT_SUCCESS)
				error = git_iterator_advance(old, &oitem);
			continue;
		}

		/* create ADDED, TRACKED, or IGNORED records for new items not
		 * matched in old (and/or descend into directories as needed)
		 */
		if (nitem && (!oitem || strcmp(oitem->path, nitem->path) > 0)) {
			int is_ignored;
			git_status_t use_status = GIT_STATUS_ADDED;

			/* contained in ignored parent directory, so this can be skipped. */
			if (ignore_prefix != NULL &&
				git__prefixcmp(nitem->path, ignore_prefix) == 0)
			{
				error = git_iterator_advance(new, &nitem);
				continue;
			}

			is_ignored = git_iterator_current_is_ignored(new);

			if (S_ISDIR(nitem->mode)) {
				if (git__prefixcmp(oitem->path, nitem->path) == 0) {
					if (is_ignored)
						ignore_prefix = nitem->path;
					error = git_iterator_advance_into_directory(new, &nitem);
					continue;
				}
				use_status = GIT_STATUS_UNTRACKED;
			}
			else if (is_ignored)
				use_status = GIT_STATUS_IGNORED;
			else if (new->type == GIT_ITERATOR_WORKDIR)
				use_status = GIT_STATUS_UNTRACKED;

			error = diff_delta__from_one(diff, use_status, nitem);
			if (error == GIT_SUCCESS)
				error = git_iterator_advance(new, &nitem);
			continue;
		}

		/* otherwise item paths match, so create MODIFIED record
		 * (or ADDED and DELETED pair if type changed)
		 */
		assert(oitem && nitem && strcmp(oitem->path, nitem->path) == 0);

		error = maybe_modified(old, oitem, new, nitem, diff);
		if (error == GIT_SUCCESS)
			error = git_iterator_advance(old, &oitem);
		if (error == GIT_SUCCESS)
			error = git_iterator_advance(new, &nitem);
	}

cleanup:
	git_iterator_free(old);
	git_iterator_free(new);

	if (error != GIT_SUCCESS) {
		git_diff_list_free(diff);
		diff = NULL;
	}

	*diff_ptr = diff;

	return error;
}


int git_diff_tree_to_tree(
	git_repository *repo,
	const git_diff_options *opts, /**< can be NULL for defaults */
	git_tree *old,
	git_tree *new,
	git_diff_list **diff)
{
	int error;
	git_iterator *a = NULL, *b = NULL;

	assert(repo && old && new && diff);

	if ((error = git_iterator_for_tree(repo, old, &a)) < GIT_SUCCESS ||
		(error = git_iterator_for_tree(repo, new, &b)) < GIT_SUCCESS)
		return error;

	return diff_from_iterators(repo, opts, a, b, diff);
}

int git_diff_index_to_tree(
	git_repository *repo,
	const git_diff_options *opts,
	git_tree *old,
	git_diff_list **diff)
{
	int error;
	git_iterator *a = NULL, *b = NULL;

	assert(repo && old && diff);

	if ((error = git_iterator_for_tree(repo, old, &a)) < GIT_SUCCESS ||
		(error = git_iterator_for_index(repo, &b)) < GIT_SUCCESS)
		return error;

	return diff_from_iterators(repo, opts, a, b, diff);
}

int git_diff_workdir_to_index(
	git_repository *repo,
	const git_diff_options *opts,
	git_diff_list **diff)
{
	int error;
	git_iterator *a = NULL, *b = NULL;

	assert(repo && diff);

	if ((error = git_iterator_for_index(repo, &a)) < GIT_SUCCESS ||
		(error = git_iterator_for_workdir(repo, &b)) < GIT_SUCCESS)
		return error;

	return diff_from_iterators(repo, opts, a, b, diff);
}


int git_diff_workdir_to_tree(
	git_repository *repo,
	const git_diff_options *opts,
	git_tree *old,
	git_diff_list **diff)
{
	int error;
	git_iterator *a = NULL, *b = NULL;

	assert(repo && old && diff);

	if ((error = git_iterator_for_tree(repo, old, &a)) < GIT_SUCCESS ||
		(error = git_iterator_for_workdir(repo, &b)) < GIT_SUCCESS)
		return error;

	return diff_from_iterators(repo, opts, a, b, diff);
}

int git_diff_merge(
	git_diff_list *onto,
	const git_diff_list *from)
{
	int error;
	unsigned int i = 0, j = 0;
	git_vector onto_new;
	git_diff_delta *delta;

	error = git_vector_init(&onto_new, onto->deltas.length, diff_delta__cmp);
	if (error < GIT_SUCCESS)
		return error;

	while (i < onto->deltas.length || j < from->deltas.length) {
		git_diff_delta       *o = git_vector_get(&onto->deltas, i);
		const git_diff_delta *f = git_vector_get_const(&from->deltas, j);
		const char *opath = !o ? NULL : o->old.path ? o->old.path : o->new.path;
		const char *fpath = !f ? NULL : f->old.path ? f->old.path : f->new.path;

		if (opath && (!fpath || strcmp(opath, fpath) < 0)) {
			delta = diff_delta__dup(o);
			i++;
		} else if (fpath && (!opath || strcmp(opath, fpath) > 0)) {
			delta = diff_delta__dup(f);
			j++;
		} else {
			delta = diff_delta__merge_like_cgit(o, f);
			i++;
			j++;
		}

		if (!delta)
			error = GIT_ENOMEM;
		else
			error = git_vector_insert(&onto_new, delta);

		if (error != GIT_SUCCESS)
			break;
	}

	if (error == GIT_SUCCESS) {
		git_vector_swap(&onto->deltas, &onto_new);
		onto->new_src = from->new_src;
	}

	git_vector_foreach(&onto_new, i, delta)
		diff_delta__free(delta);
	git_vector_free(&onto_new);

	return error;
}
