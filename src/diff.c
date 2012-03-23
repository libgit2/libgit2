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
	git_delta_t status,
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
		case GIT_DELTA_ADDED:   status = GIT_DELTA_DELETED; break;
		case GIT_DELTA_DELETED: status = GIT_DELTA_ADDED; break;
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
		if (dup->status == GIT_DELTA_DELETED)
			/* preserve pending delete info */;
		else if (b->status == GIT_DELTA_UNTRACKED ||
				 b->status == GIT_DELTA_IGNORED)
			dup->status = b->status;
		else
			dup->status = GIT_DELTA_UNMODIFIED;
	}
	else if (dup->status == GIT_DELTA_UNMODIFIED ||
			 b->status == GIT_DELTA_DELETED)
		dup->status = b->status;

	return dup;
}

static int diff_delta__from_one(
	git_diff_list *diff,
	git_delta_t   status,
	const git_index_entry *entry)
{
	git_diff_delta *delta;

	if (status == GIT_DELTA_IGNORED &&
		(diff->opts.flags & GIT_DIFF_INCLUDE_IGNORED) == 0)
		return 0;

	if (status == GIT_DELTA_UNTRACKED &&
		(diff->opts.flags & GIT_DIFF_INCLUDE_UNTRACKED) == 0)
		return 0;

	delta = diff_delta__alloc(diff, status, entry->path);
	GITERR_CHECK_ALLOC(delta);

	/* This fn is just for single-sided diffs */
	assert(status != GIT_DELTA_MODIFIED);

	if (delta->status == GIT_DELTA_DELETED) {
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

	if (git_vector_insert(&diff->deltas, delta) < 0) {
		diff_delta__free(delta);
		return -1;
	}

	return 0;
}

static int diff_delta__from_two(
	git_diff_list *diff,
	git_delta_t   status,
	const git_index_entry *old,
	const git_index_entry *new,
	git_oid *new_oid)
{
	git_diff_delta *delta;

	if (status == GIT_DELTA_UNMODIFIED &&
		(diff->opts.flags & GIT_DIFF_INCLUDE_UNMODIFIED) == 0)
		return 0;

	if ((diff->opts.flags & GIT_DIFF_REVERSE) != 0) {
		const git_index_entry *temp = old;
		old = new;
		new = temp;
	}

	delta = diff_delta__alloc(diff, status, old->path);
	GITERR_CHECK_ALLOC(delta);

	delta->old.mode = old->mode;
	git_oid_cpy(&delta->old.oid, &old->oid);
	delta->old.flags |= GIT_DIFF_FILE_VALID_OID;

	delta->new.mode = new->mode;
	git_oid_cpy(&delta->new.oid, new_oid ? new_oid : &new->oid);
	if (new_oid || !git_oid_iszero(&new->oid))
		delta->new.flags |= GIT_DIFF_FILE_VALID_OID;

	if (git_vector_insert(&diff->deltas, delta) < 0) {
		diff_delta__free(delta);
		return -1;
	}

	return 0;
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

	if (git_vector_init(&diff->deltas, 0, diff_delta__cmp) < 0) {
		git__free(diff->opts.src_prefix);
		git__free(diff->opts.dst_prefix);
		git__free(diff);
		return NULL;
	}

	/* TODO: do something safe with the pathspec strarray */

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
	int result;
	git_buf full_path = GIT_BUF_INIT;

	if (git_buf_joinpath(&full_path, git_repository_workdir(repo), item->path) < 0)
		return -1;

	/* calculate OID for file if possible*/
	if (S_ISLNK(item->mode))
		result = git_odb__hashlink(oid, full_path.ptr);
	else if (!git__is_sizet(item->file_size)) {
		giterr_set(GITERR_OS, "File size overflow for 32-bit systems");
		result = -1;
	} else {
		int fd = git_futils_open_ro(full_path.ptr);
		if (fd < 0)
			result = fd;
		else {
			result = git_odb__hashfd(
				oid, fd, (size_t)item->file_size, GIT_OBJ_BLOB);
			p_close(fd);
		}
	}

	git_buf_free(&full_path);

	return result;
}

static int maybe_modified(
	git_iterator *old,
	const git_index_entry *oitem,
	git_iterator *new,
	const git_index_entry *nitem,
	git_diff_list *diff)
{
	git_oid noid, *use_noid = NULL;
	git_delta_t status = GIT_DELTA_MODIFIED;

	GIT_UNUSED(old);

	/* support "assume unchanged" & "skip worktree" bits */
	if ((oitem->flags_extended & GIT_IDXENTRY_INTENT_TO_ADD) != 0 ||
		(oitem->flags_extended & GIT_IDXENTRY_SKIP_WORKTREE) != 0)
		status = GIT_DELTA_UNMODIFIED;

	/* if basic type of file changed, then split into delete and add */
	else if (GIT_MODE_TYPE(oitem->mode) != GIT_MODE_TYPE(nitem->mode)) {
		if (diff_delta__from_one(diff, GIT_DELTA_DELETED, oitem) < 0 ||
			diff_delta__from_one(diff, GIT_DELTA_ADDED, nitem) < 0)
			return -1;
		return 0;
	}

	/* if oids and modes match, then file is unmodified */
	else if (git_oid_cmp(&oitem->oid, &nitem->oid) == 0 &&
			oitem->mode == nitem->mode)
		status = GIT_DELTA_UNMODIFIED;

	/* if we have a workdir item with an unknown oid, check deeper */
	else if (git_oid_iszero(&nitem->oid) && new->type == GIT_ITERATOR_WORKDIR) {
		/* if they files look exactly alike, then we'll assume the same */
		if (oitem->file_size == nitem->file_size &&
			oitem->ctime.seconds == nitem->ctime.seconds &&
			oitem->mtime.seconds == nitem->mtime.seconds &&
			oitem->dev == nitem->dev &&
			oitem->ino == nitem->ino &&
			oitem->uid == nitem->uid &&
			oitem->gid == nitem->gid)
			status = GIT_DELTA_UNMODIFIED;

		/* TODO? should we do anything special with submodules? */
		else if (S_ISGITLINK(nitem->mode))
			status = GIT_DELTA_UNMODIFIED;

		/* TODO: check git attributes so we will not have to read the file
		 * in if it is marked binary.
		 */

		else if (oid_for_workdir_item(diff->repo, nitem, &noid) < 0)
			return -1;

		else if (git_oid_cmp(&oitem->oid, &noid) == 0 &&
			oitem->mode == nitem->mode)
			status = GIT_DELTA_UNMODIFIED;

		/* store calculated oid so we don't have to recalc later */
		use_noid = &noid;
	}

	return diff_delta__from_two(diff, status, oitem, nitem, use_noid);
}

static int diff_from_iterators(
	git_repository *repo,
	const git_diff_options *opts, /**< can be NULL for defaults */
	git_iterator *old,
	git_iterator *new,
	git_diff_list **diff_ptr)
{
	const git_index_entry *oitem, *nitem;
	char *ignore_prefix = NULL;
	git_diff_list *diff = git_diff_list_alloc(repo, opts);
	if (!diff)
		goto fail;

	diff->old_src = old->type;
	diff->new_src = new->type;

	if (git_iterator_current(old, &oitem) < 0 ||
		git_iterator_current(new, &nitem) < 0)
		goto fail;

	/* run iterators building diffs */
	while (oitem || nitem) {

		/* create DELETED records for old items not matched in new */
		if (oitem && (!nitem || strcmp(oitem->path, nitem->path) < 0)) {
			if (diff_delta__from_one(diff, GIT_DELTA_DELETED, oitem) < 0 ||
				git_iterator_advance(old, &oitem) < 0)
				goto fail;
		}

		/* create ADDED, TRACKED, or IGNORED records for new items not
		 * matched in old (and/or descend into directories as needed)
		 */
		else if (nitem && (!oitem || strcmp(oitem->path, nitem->path) > 0)) {
			int is_ignored;
			git_delta_t delta_type = GIT_DELTA_ADDED;

			/* contained in ignored parent directory, so this can be skipped. */
			if (ignore_prefix != NULL &&
				git__prefixcmp(nitem->path, ignore_prefix) == 0)
			{
				if (git_iterator_advance(new, &nitem) < 0)
					goto fail;
				continue;
			}

			is_ignored = git_iterator_current_is_ignored(new);

			if (S_ISDIR(nitem->mode)) {
				/* recurse into directory if explicitly requested or
				 * if there are tracked items inside the directory
				 */
				if ((diff->opts.flags & GIT_DIFF_RECURSE_UNTRACKED_DIRS) ||
					(oitem && git__prefixcmp(oitem->path, nitem->path) == 0))
				{
					if (is_ignored)
						ignore_prefix = nitem->path;
					if (git_iterator_advance_into_directory(new, &nitem) < 0)
						goto fail;
					continue;
				}
				delta_type = GIT_DELTA_UNTRACKED;
			}
			else if (is_ignored)
				delta_type = GIT_DELTA_IGNORED;
			else if (new->type == GIT_ITERATOR_WORKDIR)
				delta_type = GIT_DELTA_UNTRACKED;

			if (diff_delta__from_one(diff, delta_type, nitem) < 0 ||
				git_iterator_advance(new, &nitem) < 0)
				goto fail;
		}

		/* otherwise item paths match, so create MODIFIED record
		 * (or ADDED and DELETED pair if type changed)
		 */
		else {
			assert(oitem && nitem && strcmp(oitem->path, nitem->path) == 0);

			if (maybe_modified(old, oitem, new, nitem, diff) < 0 ||
				git_iterator_advance(old, &oitem) < 0 ||
				git_iterator_advance(new, &nitem) < 0)
				goto fail;
		}
	}

	git_iterator_free(old);
	git_iterator_free(new);
	*diff_ptr = diff;
	return 0;

fail:
	git_iterator_free(old);
	git_iterator_free(new);
	git_diff_list_free(diff);
	*diff_ptr = NULL;
	return -1;
}


int git_diff_tree_to_tree(
	git_repository *repo,
	const git_diff_options *opts, /**< can be NULL for defaults */
	git_tree *old,
	git_tree *new,
	git_diff_list **diff)
{
	git_iterator *a = NULL, *b = NULL;

	assert(repo && old && new && diff);

	if (git_iterator_for_tree(repo, old, &a) < 0 ||
		git_iterator_for_tree(repo, new, &b) < 0)
		return -1;

	return diff_from_iterators(repo, opts, a, b, diff);
}

int git_diff_index_to_tree(
	git_repository *repo,
	const git_diff_options *opts,
	git_tree *old,
	git_diff_list **diff)
{
	git_iterator *a = NULL, *b = NULL;

	assert(repo && old && diff);

	if (git_iterator_for_tree(repo, old, &a) < 0 ||
		git_iterator_for_index(repo, &b) < 0)
		return -1;

	return diff_from_iterators(repo, opts, a, b, diff);
}

int git_diff_workdir_to_index(
	git_repository *repo,
	const git_diff_options *opts,
	git_diff_list **diff)
{
	git_iterator *a = NULL, *b = NULL;

	assert(repo && diff);

	if (git_iterator_for_index(repo, &a) < 0 ||
		git_iterator_for_workdir(repo, &b) < 0)
		return -1;

	return diff_from_iterators(repo, opts, a, b, diff);
}


int git_diff_workdir_to_tree(
	git_repository *repo,
	const git_diff_options *opts,
	git_tree *old,
	git_diff_list **diff)
{
	git_iterator *a = NULL, *b = NULL;

	assert(repo && old && diff);

	if (git_iterator_for_tree(repo, old, &a) < 0 ||
		git_iterator_for_workdir(repo, &b) < 0)
		return -1;

	return diff_from_iterators(repo, opts, a, b, diff);
}

int git_diff_merge(
	git_diff_list *onto,
	const git_diff_list *from)
{
	int error = 0;
	git_vector onto_new;
	git_diff_delta *delta, *o;
	const git_diff_delta *f;
	unsigned int i;

	if (git_vector_init(&onto_new, onto->deltas.length, diff_delta__cmp) < 0)
		return -1;

	GIT_DIFF_COITERATE(
		onto, from, o, f,
		delta = diff_delta__dup(o),
		delta = diff_delta__dup(f),
		delta = diff_delta__merge_like_cgit(o, f),
		if ((error = !delta ? -1 : git_vector_insert(&onto_new, delta)) < 0)
			break;
		);

	if (error == 0) {
		git_vector_swap(&onto->deltas, &onto_new);
		onto->new_src = from->new_src;
	}

	git_vector_foreach(&onto_new, i, delta)
		diff_delta__free(delta);
	git_vector_free(&onto_new);

	return error;
}

