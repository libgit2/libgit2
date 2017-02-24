/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"

#include "git2/branch.h"
#include "git2/commit.h"
#include "git2/worktree.h"

#include "repository.h"
#include "worktree.h"

static bool is_worktree_dir(git_buf *dir)
{
	return git_path_contains_file(dir, "commondir")
		&& git_path_contains_file(dir, "gitdir")
		&& git_path_contains_file(dir, "HEAD");
}

int git_worktree_list(git_strarray *wts, git_repository *repo)
{
	git_vector worktrees = GIT_VECTOR_INIT;
	git_buf path = GIT_BUF_INIT;
	char *worktree;
	unsigned i, len;
	int error;

	assert(wts && repo);

	wts->count = 0;
	wts->strings = NULL;

	if ((error = git_buf_printf(&path, "%s/worktrees/", repo->commondir)) < 0)
		goto exit;
	if (!git_path_exists(path.ptr) || git_path_is_empty_dir(path.ptr))
		goto exit;
	if ((error = git_path_dirload(&worktrees, path.ptr, path.size, 0x0)) < 0)
		goto exit;

	len = path.size;

	git_vector_foreach(&worktrees, i, worktree) {
		git_buf_truncate(&path, len);
		git_buf_puts(&path, worktree);

		if (!is_worktree_dir(&path)) {
			git_vector_remove(&worktrees, i);
			git__free(worktree);
		}
	}

	wts->strings = (char **)git_vector_detach(&wts->count, NULL, &worktrees);

exit:
	git_buf_free(&path);

	return error;
}

char *git_worktree__read_link(const char *base, const char *file)
{
	git_buf path = GIT_BUF_INIT, buf = GIT_BUF_INIT;

	assert(base && file);

	if (git_buf_joinpath(&path, base, file) < 0)
		goto err;
	if (git_futils_readbuffer(&buf, path.ptr) < 0)
		goto err;
	git_buf_free(&path);

	git_buf_rtrim(&buf);

	if (!git_path_is_relative(buf.ptr))
		return git_buf_detach(&buf);

	if (git_buf_sets(&path, base) < 0)
		goto err;
	if (git_path_apply_relative(&path, buf.ptr) < 0)
		goto err;
	git_buf_free(&buf);

	return git_buf_detach(&path);

err:
	git_buf_free(&buf);
	git_buf_free(&path);

	return NULL;
}

static int write_wtfile(const char *base, const char *file, const git_buf *buf)
{
	git_buf path = GIT_BUF_INIT;
	int err;

	assert(base && file && buf);

	if ((err = git_buf_joinpath(&path, base, file)) < 0)
		goto out;

	if ((err = git_futils_writebuffer(buf, path.ptr, O_CREAT|O_EXCL|O_WRONLY, 0644)) < 0)
		goto out;

out:
	git_buf_free(&path);

	return err;
}

int git_worktree_lookup(git_worktree **out, git_repository *repo, const char *name)
{
	git_buf path = GIT_BUF_INIT;
	git_worktree *wt = NULL;
	int error;

	assert(repo && name);

	*out = NULL;

	if ((error = git_buf_printf(&path, "%s/worktrees/%s", repo->commondir, name)) < 0)
		goto out;

	if (!is_worktree_dir(&path)) {
		error = -1;
		goto out;
	}

	if ((wt = git__malloc(sizeof(struct git_repository))) == NULL) {
		error = -1;
		goto out;
	}

	if ((wt->name = git__strdup(name)) == NULL
	    || (wt->commondir_path = git_worktree__read_link(path.ptr, "commondir")) == NULL
	    || (wt->gitlink_path = git_worktree__read_link(path.ptr, "gitdir")) == NULL
	    || (wt->parent_path = git__strdup(git_repository_path(repo))) == NULL) {
		error = -1;
		goto out;
	}
	wt->gitdir_path = git_buf_detach(&path);
	wt->locked = !!git_worktree_is_locked(NULL, wt);

	(*out) = wt;

out:
	git_buf_free(&path);

	if (error)
		git_worktree_free(wt);

	return error;
}

void git_worktree_free(git_worktree *wt)
{
	if (!wt)
		return;

	git__free(wt->commondir_path);
	git__free(wt->gitlink_path);
	git__free(wt->gitdir_path);
	git__free(wt->parent_path);
	git__free(wt->name);
	git__free(wt);
}

int git_worktree_validate(const git_worktree *wt)
{
	git_buf buf = GIT_BUF_INIT;
	int err = 0;

	assert(wt);

	git_buf_puts(&buf, wt->gitdir_path);
	if (!is_worktree_dir(&buf)) {
		giterr_set(GITERR_WORKTREE,
			"Worktree gitdir ('%s') is not valid",
			wt->gitlink_path);
		err = -1;
		goto out;
	}

	if (!git_path_exists(wt->parent_path)) {
		giterr_set(GITERR_WORKTREE,
			"Worktree parent directory ('%s') does not exist ",
			wt->parent_path);
		err = -2;
		goto out;
	}

	if (!git_path_exists(wt->commondir_path)) {
		giterr_set(GITERR_WORKTREE,
			"Worktree common directory ('%s') does not exist ",
			wt->commondir_path);
		err = -3;
		goto out;
	}

out:
	git_buf_free(&buf);

	return err;
}

int git_worktree_add(git_worktree **out, git_repository *repo, const char *name, const char *worktree)
{
	git_buf path = GIT_BUF_INIT, buf = GIT_BUF_INIT;
	git_reference *ref = NULL, *head = NULL;
	git_commit *commit = NULL;
	git_repository *wt = NULL;
	git_checkout_options coopts = GIT_CHECKOUT_OPTIONS_INIT;
	int err;

	assert(out && repo && name && worktree);

	*out = NULL;

	/* Create worktree related files in commondir */
	if ((err = git_buf_joinpath(&path, repo->commondir, "worktrees")) < 0)
		goto out;
	if (!git_path_exists(path.ptr))
		if ((err = git_futils_mkdir(path.ptr, 0755, GIT_MKDIR_EXCL)) < 0)
			goto out;
	if ((err = git_buf_joinpath(&path, path.ptr, name)) < 0)
		goto out;
	if ((err = git_futils_mkdir(path.ptr, 0755, GIT_MKDIR_EXCL)) < 0)
		goto out;

	/* Create worktree work dir */
	if ((err = git_futils_mkdir(worktree, 0755, GIT_MKDIR_EXCL)) < 0)
		goto out;

	/* Create worktree .git file */
	if ((err = git_buf_printf(&buf, "gitdir: %s\n", path.ptr)) < 0)
		goto out;
	if ((err = write_wtfile(worktree, ".git", &buf)) < 0)
		goto out;

	/* Create commondir files */
	if ((err = git_buf_sets(&buf, repo->commondir)) < 0
	    || (err = git_buf_putc(&buf, '\n')) < 0
	    || (err = write_wtfile(path.ptr, "commondir", &buf)) < 0)
		goto out;
	if ((err = git_buf_joinpath(&buf, worktree, ".git")) < 0
	    || (err = git_buf_putc(&buf, '\n')) < 0
	    || (err = write_wtfile(path.ptr, "gitdir", &buf)) < 0)
		goto out;

	/* Create new branch */
	if ((err = git_repository_head(&head, repo)) < 0)
		goto out;
	if ((err = git_commit_lookup(&commit, repo, &head->target.oid)) < 0)
		goto out;
	if ((err = git_branch_create(&ref, repo, name, commit, false)) < 0)
		goto out;

	/* Set worktree's HEAD */
	if ((err = git_repository_create_head(path.ptr, name)) < 0)
		goto out;
	if ((err = git_repository_open(&wt, worktree)) < 0)
		goto out;

	/* Checkout worktree's HEAD */
	coopts.checkout_strategy = GIT_CHECKOUT_FORCE;
	if ((err = git_checkout_head(wt, &coopts)) < 0)
		goto out;

	/* Load result */
	if ((err = git_worktree_lookup(out, repo, name)) < 0)
		goto out;

out:
	git_buf_free(&path);
	git_buf_free(&buf);
	git_reference_free(ref);
	git_reference_free(head);
	git_commit_free(commit);
	git_repository_free(wt);

	return err;
}

int git_worktree_lock(git_worktree *wt, char *creason)
{
	git_buf buf = GIT_BUF_INIT, path = GIT_BUF_INIT;
	int err;

	assert(wt);

	if ((err = git_worktree_is_locked(NULL, wt)) < 0)
		goto out;

	if ((err = git_buf_joinpath(&path, wt->gitdir_path, "locked")) < 0)
		goto out;

	if (creason)
		git_buf_attach_notowned(&buf, creason, strlen(creason));

	if ((err = git_futils_writebuffer(&buf, path.ptr, O_CREAT|O_EXCL|O_WRONLY, 0644)) < 0)
		goto out;

	wt->locked = 1;

out:
	git_buf_free(&path);

	return err;
}

int git_worktree_unlock(git_worktree *wt)
{
	git_buf path = GIT_BUF_INIT;

	assert(wt);

	if (!git_worktree_is_locked(NULL, wt))
		return 0;

	if (git_buf_joinpath(&path, wt->gitdir_path, "locked") < 0)
		return -1;

	if (p_unlink(path.ptr) != 0) {
		git_buf_free(&path);
		return -1;
	}

	wt->locked = 0;

	git_buf_free(&path);

	return 0;
}

int git_worktree_is_locked(git_buf *reason, const git_worktree *wt)
{
	git_buf path = GIT_BUF_INIT;
	int ret;

	assert(wt);

	if (reason)
		git_buf_clear(reason);

	if ((ret = git_buf_joinpath(&path, wt->gitdir_path, "locked")) < 0)
		goto out;
	if ((ret = git_path_exists(path.ptr)) && reason)
		git_futils_readbuffer(reason, path.ptr);

out:
	git_buf_free(&path);

	return ret;
}

int git_worktree_is_prunable(git_worktree *wt, unsigned flags)
{
	git_buf reason = GIT_BUF_INIT;

	if ((flags & GIT_WORKTREE_PRUNE_LOCKED) == 0 &&
		git_worktree_is_locked(&reason, wt))
	{
		if (!reason.size)
			git_buf_attach_notowned(&reason, "no reason given", 15);
		giterr_set(GITERR_WORKTREE, "Not pruning locked working tree: '%s'", reason.ptr);
		git_buf_free(&reason);

		return 0;
	}

	if ((flags & GIT_WORKTREE_PRUNE_VALID) == 0 &&
		git_worktree_validate(wt) == 0)
	{
		giterr_set(GITERR_WORKTREE, "Not pruning valid working tree");
		return 0;
	}

	return 1;
}

int git_worktree_prune(git_worktree *wt, unsigned flags)
{
	git_buf path = GIT_BUF_INIT;
	char *wtpath;
	int err;

	if (!git_worktree_is_prunable(wt, flags)) {
		err = -1;
		goto out;
	}

	/* Delete gitdir in parent repository */
	if ((err = git_buf_printf(&path, "%s/worktrees/%s", wt->parent_path, wt->name)) < 0)
		goto out;
	if (!git_path_exists(path.ptr))
	{
		giterr_set(GITERR_WORKTREE, "Worktree gitdir '%s' does not exist", path.ptr);
		err = -1;
		goto out;
	}
	if ((err = git_futils_rmdir_r(path.ptr, NULL, GIT_RMDIR_REMOVE_FILES)) < 0)
		goto out;

	/* Skip deletion of the actual working tree if it does
	 * not exist or deletion was not requested */
	if ((flags & GIT_WORKTREE_PRUNE_WORKING_TREE) == 0 ||
		!git_path_exists(wt->gitlink_path))
	{
		goto out;
	}

	if ((wtpath = git_path_dirname(wt->gitlink_path)) == NULL)
		goto out;
	git_buf_attach(&path, wtpath, 0);
	if (!git_path_exists(path.ptr))
	{
		giterr_set(GITERR_WORKTREE, "Working tree '%s' does not exist", path.ptr);
		err = -1;
		goto out;
	}
	if ((err = git_futils_rmdir_r(path.ptr, NULL, GIT_RMDIR_REMOVE_FILES)) < 0)
		goto out;

out:
	git_buf_free(&path);

	return err;
}
