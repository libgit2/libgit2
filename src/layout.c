/*
* Copyright (C) the libgit2 contributors. All rights reserved.
*
* This file is part of libgit2, distributed under the GNU GPL v2 with
* a Linking Exception. For full terms see the included COPYING file.
*/

#include "layout.h"

#include "odb.h"
#include "worktree.h"

static const struct {
	git_repository_item_t parent;
	git_repository_item_t fallback;
	const char *name;
	bool directory;
} items[] = {
	{ GIT_REPOSITORY_ITEM_GITDIR, GIT_REPOSITORY_ITEM__LAST, NULL, true },			/* GIT_REPOSITORY_ITEM_GITDIR */
	{ GIT_REPOSITORY_ITEM_WORKDIR, GIT_REPOSITORY_ITEM__LAST, NULL, true },			/* GIT_REPOSITORY_ITEM_WORKDIR */
	{ GIT_REPOSITORY_ITEM_COMMONDIR, GIT_REPOSITORY_ITEM__LAST, NULL, true },		/* GIT_REPOSITORY_ITEM_COMMONDIR */
	{ GIT_REPOSITORY_ITEM_GITDIR, GIT_REPOSITORY_ITEM__LAST, "index", false },		/* GIT_REPOSITORY_ITEM_INDEX */
	{ GIT_REPOSITORY_ITEM_COMMONDIR, GIT_REPOSITORY_ITEM_GITDIR, "objects", true },		/* GIT_REPOSITORY_ITEM_OBJECTS */
	{ GIT_REPOSITORY_ITEM_COMMONDIR, GIT_REPOSITORY_ITEM_GITDIR, "refs", true },		/* GIT_REPOSITORY_ITEM_REFS */
	{ GIT_REPOSITORY_ITEM_COMMONDIR, GIT_REPOSITORY_ITEM_GITDIR, "packed-refs", false },	/* GIT_REPOSITORY_ITEM_PACKED_REFS */
	{ GIT_REPOSITORY_ITEM_COMMONDIR, GIT_REPOSITORY_ITEM_GITDIR, "remotes", true },		/* GIT_REPOSITORY_ITEM_REMOTES */
	{ GIT_REPOSITORY_ITEM_COMMONDIR, GIT_REPOSITORY_ITEM_GITDIR, "config", false },		/* GIT_REPOSITORY_ITEM_CONFIG */
	{ GIT_REPOSITORY_ITEM_COMMONDIR, GIT_REPOSITORY_ITEM_GITDIR, "info", true },		/* GIT_REPOSITORY_ITEM_INFO */
	{ GIT_REPOSITORY_ITEM_COMMONDIR, GIT_REPOSITORY_ITEM_GITDIR, "hooks", true },		/* GIT_REPOSITORY_ITEM_HOOKS */
	{ GIT_REPOSITORY_ITEM_COMMONDIR, GIT_REPOSITORY_ITEM_GITDIR, "logs", true },		/* GIT_REPOSITORY_ITEM_LOGS */
	{ GIT_REPOSITORY_ITEM_GITDIR, GIT_REPOSITORY_ITEM__LAST, "modules", true },		/* GIT_REPOSITORY_ITEM_MODULES */
	{ GIT_REPOSITORY_ITEM_COMMONDIR, GIT_REPOSITORY_ITEM_GITDIR, "worktrees", true },	/* GIT_REPOSITORY_ITEM_WORKTREES */
	{ GIT_REPOSITORY_ITEM_GITDIR, GIT_REPOSITORY_ITEM__LAST, GIT_CHERRYPICK_HEAD_FILE, false }, /* GIT_REPOSITORY_ITEM_CHERRYPICK_HEAD */
	{ GIT_REPOSITORY_ITEM_GITDIR, GIT_REPOSITORY_ITEM__LAST, GIT_FETCH_HEAD_FILE, false },	/* GIT_REPOSITORY_ITEM_FETCH_HEAD */
	{ GIT_REPOSITORY_ITEM_GITDIR, GIT_REPOSITORY_ITEM__LAST, GIT_HEAD_FILE, false },	/* GIT_REPOSITORY_ITEM_HEAD */
	{ GIT_REPOSITORY_ITEM_GITDIR, GIT_REPOSITORY_ITEM__LAST, GIT_MERGE_HEAD_FILE, false },	/* GIT_REPOSITORY_ITEM_MERGE_HEAD */
	{ GIT_REPOSITORY_ITEM_GITDIR, GIT_REPOSITORY_ITEM__LAST, GIT_MERGE_MODE_FILE, false },	/* GIT_REPOSITORY_ITEM_MERGE_MODE */
	{ GIT_REPOSITORY_ITEM_GITDIR, GIT_REPOSITORY_ITEM__LAST, GIT_MERGE_MSG_FILE, false },	/* GIT_REPOSITORY_ITEM_MERGE_MSG */
	{ GIT_REPOSITORY_ITEM_GITDIR, GIT_REPOSITORY_ITEM__LAST, GIT_ORIG_HEAD_FILE, false },	/* GIT_REPOSITORY_ITEM_ORIG_HEAD */
	{ GIT_REPOSITORY_ITEM_GITDIR, GIT_REPOSITORY_ITEM__LAST, GIT_REBASE_APPLY_DIR, true },	/* GIT_REPOSITORY_ITEM_REBASE_APPLY */
	{ GIT_REPOSITORY_ITEM_GITDIR, GIT_REPOSITORY_ITEM__LAST, GIT_REBASE_MERGE_DIR, true },	/* GIT_REPOSITORY_ITEM_REBASE_MERGE */
	{ GIT_REPOSITORY_ITEM_GITDIR, GIT_REPOSITORY_ITEM__LAST, GIT_REVERT_HEAD_FILE, false }	/* GIT_REPOSITORY_ITEM_REVERT_HEAD */
};

static const char *resolved_parent_path(const git_repository_layout *layout, git_repository_item_t item, git_repository_item_t fallback)
{
	const char *parent;

	switch (item) {
		case GIT_REPOSITORY_ITEM_GITDIR:
			parent = layout->gitdir;
			break;
		case GIT_REPOSITORY_ITEM_WORKDIR:
			parent = layout->workdir;
			break;
		case GIT_REPOSITORY_ITEM_COMMONDIR:
			parent = layout->commondir;
			break;
		default:
			git_error_set(GIT_ERROR_INVALID, "invalid item directory");
			return NULL;
	}
	if (!parent && fallback != GIT_REPOSITORY_ITEM__LAST)
		return resolved_parent_path(layout, fallback, GIT_REPOSITORY_ITEM__LAST);

	return parent;
}

int git_layout_item_path(git_buf *out, const git_repository_layout *layout, git_repository_item_t item)
{
	const char *parent = resolved_parent_path(layout, items[item].parent, items[item].fallback);
	if (parent == NULL) {
		git_error_set(GIT_ERROR_INVALID, "path cannot exist in repository");
		return GIT_ENOTFOUND;
	}

	if (git_buf_sets(out, parent) < 0)
		return -1;

	if (items[item].name) {
		if (git_buf_joinpath(out, parent, items[item].name) < 0)
			return -1;
	}

	if (items[item].directory) {
		if (git_path_to_dir(out) < 0)
			return -1;
	}

	return 0;
}

/*
 * Git repository open methods
 *
 * Open a repository object from its path
 */
bool git_layout_is_valid_repository(git_buf *repository_path, git_buf *common_path)
{
	/* Check if we have a separate commondir (e.g. we have a
	 * worktree) */
	if (git_path_contains_file(repository_path, GIT_COMMONDIR_FILE)) {
		git_buf common_link  = GIT_BUF_INIT;
		git_buf_joinpath(&common_link, repository_path->ptr, GIT_COMMONDIR_FILE);

		git_futils_readbuffer(&common_link, common_link.ptr);
		git_buf_rtrim(&common_link);

		if (git_path_is_relative(common_link.ptr)) {
			git_buf_joinpath(common_path, repository_path->ptr, common_link.ptr);
		} else {
			git_buf_swap(common_path, &common_link);
		}

		git_buf_dispose(&common_link);
	}
	else {
		git_buf_set(common_path, repository_path->ptr, repository_path->size);
	}

	/* Make sure the commondir path always has a trailing * slash */
	if (git_buf_rfind(common_path, '/') != (ssize_t)common_path->size - 1)
		git_buf_putc(common_path, '/');

	/* Ensure HEAD file exists */
	if (git_path_contains_file(repository_path, GIT_HEAD_FILE) == false)
		return false;

	/* Check files in common dir */
	if (git_path_contains_dir(common_path, GIT_OBJECTS_DIR) == false)
		return false;
	if (git_path_contains_dir(common_path, GIT_REFS_DIR) == false)
		return false;

	return true;
}

/*
 * This function returns furthest offset into path where a ceiling dir
 * is found, so we can stop processing the path at that point.
 *
 * Note: converting this to use git_bufs instead of GIT_PATH_MAX buffers on
 * the stack could remove directories name limits, but at the cost of doing
 * repeated malloc/frees inside the loop below, so let's not do it now.
 */
static size_t find_ceiling_dir_offset(
	const char *path,
	const git_strarray *ceiling_directories)
{
	char real_dir[GIT_PATH_MAX + 1];
	char *dir;
	size_t len, max_len = 0, min_len, cur;

	assert(path);

	min_len = (size_t)(git_path_root(path) + 1);

	if (ceiling_directories->count == 0 || min_len == 0)
		return min_len;

	for (cur = 0; cur < ceiling_directories->count; cur++) {
		dir = ceiling_directories->strings[cur];
		len = strlen(dir);

		if (len == 0 || git_path_root(dir) == -1)
			continue;

		if (p_realpath(dir, real_dir) == NULL)
			continue;

		len = strlen(real_dir);
		if (len > 0 && real_dir[len-1] == '/')
			dir[--len] = '\0';

		if (!strncmp(path, real_dir, len) &&
			(path[len] == '/' || !path[len]) &&
			len > max_len)
		{
			max_len = len;
		}
	}

	return (max_len <= min_len ? min_len : max_len);
}

static int read_gitfile(git_buf *path_out, const char *file_path);

int git_layout_find_repo(
	git_repository_layout *layout,
	const char *start_path,
	uint32_t flags,
	git_strarray *ceiling_dirs)
{
	int error;
	git_buf gitdir = GIT_BUF_INIT;
	git_buf repo_link = GIT_BUF_INIT;
	git_buf common_link = GIT_BUF_INIT;
	struct stat st;
	dev_t initial_device = 0;
	int min_iterations;
	bool in_dot_git;
	size_t ceiling_offset = 0;

	memset(layout, 0, sizeof(*layout));

	error = git_path_prettify(&gitdir, start_path, NULL);
	if (error < 0)
		return error;

	/* in_dot_git toggles each loop:
	 * /a/b/c/.git, /a/b/c, /a/b/.git, /a/b, /a/.git, /a
	 * With GIT_REPOSITORY_OPEN_BARE or GIT_REPOSITORY_OPEN_NO_DOTGIT, we
	 * assume we started with /a/b/c.git and don't append .git the first
	 * time through.
	 * min_iterations indicates the number of iterations left before going
	 * further counts as a search. */
	if (flags & (GIT_REPOSITORY_OPEN_BARE | GIT_REPOSITORY_OPEN_NO_DOTGIT)) {
		in_dot_git = true;
		min_iterations = 1;
	} else {
		in_dot_git = false;
		min_iterations = 2;
	}

	for (;;) {
		if (!(flags & GIT_REPOSITORY_OPEN_NO_DOTGIT)) {
			if (!in_dot_git) {
				error = git_buf_joinpath(&gitdir, gitdir.ptr, DOT_GIT);
				if (error < 0)
					break;
			}
			in_dot_git = !in_dot_git;
		}

		if (p_stat(gitdir.ptr, &st) == 0) {
			/* check that we have not crossed device boundaries */
			if (initial_device == 0)
				initial_device = st.st_dev;
			else if (st.st_dev != initial_device &&
					 !(flags & GIT_REPOSITORY_OPEN_CROSS_FS))
				break;

			if (S_ISDIR(st.st_mode) && git_layout_is_valid_repository(&gitdir, &common_link)) {
				git_path_to_dir(&gitdir);

				layout->gitdir = git__strndup(gitdir.ptr, git_buf_len(&gitdir));
				layout->gitlink = git_worktree__read_link(layout->gitdir, GIT_GITDIR_FILE);
				layout->commondir = git_buf_detach(&common_link);
				break;
			}
			else if (S_ISREG(st.st_mode) && git__suffixcmp(gitdir.ptr, "/" DOT_GIT) == 0) {
				error = read_gitfile(&repo_link, gitdir.ptr);
				if (error < 0)
					break;

				if (git_layout_is_valid_repository(&repo_link, &common_link)) {
					git_buf_swap(&gitdir, &repo_link);

					layout->gitdir = git__strndup(gitdir.ptr, git_buf_len(&gitdir));
					layout->gitlink = git__strndup(gitdir.ptr, git_buf_len(&gitdir));
					layout->commondir = git_buf_detach(&common_link);
				}
				break;
			}
		}


		/* Move up one directory. If we're in_dot_git, we'll search the
		 * parent itself next. If we're !in_dot_git, we'll search .git
		 * in the parent directory next (added at the top of the loop). */
		if (git_path_dirname_r(&gitdir, gitdir.ptr) < 0) {
			error = -1;
			break;
		}

		/* Once we've checked the directory (and .git if applicable),
		 * find the ceiling for a search. */
		if (min_iterations && (--min_iterations == 0))
			ceiling_offset = find_ceiling_dir_offset(gitdir.ptr, ceiling_dirs);

		/* Check if we should stop searching here. */
		if (min_iterations == 0
			&& (gitdir.ptr[ceiling_offset] == 0
				|| (flags & GIT_REPOSITORY_OPEN_NO_SEARCH)))
			break;
	}

	if (!error && layout->gitdir && !(flags & GIT_REPOSITORY_OPEN_BARE)) {
		git_buf workdir = GIT_BUF_INIT;
		git_path_dirname_r(&workdir, gitdir.ptr);
		git_path_to_dir(&workdir);

		if (git_buf_oom(&workdir))
			return -1;

		layout->workdir = git_buf_detach(&workdir);
	}

	/* If we didn't find the repository, and we don't have any other error
	 * to report, report that. */
	if (!layout->gitdir && !error) {
		git_error_set(GIT_ERROR_REPOSITORY,
					  "could not find repository from '%s'", start_path);
		error = GIT_ENOTFOUND;
	}

	git_buf_dispose(&gitdir);
	git_buf_dispose(&repo_link);
	git_buf_dispose(&common_link);
	return error;
}

#define GIT_GITLINK_FILE_PREFIX "gitdir:"

/*
 * Read the contents of `file_path` and set `path_out` to the repo dir that
 * it points to.  Before calling, set `path_out` to the base directory that
 * should be used if the contents of `file_path` are a relative path.
 */
static int read_gitfile(git_buf *path_out, const char *file_path)
{
	int     error = 0;
	git_buf file = GIT_BUF_INIT;
	size_t  prefix_len = strlen(GIT_GITLINK_FILE_PREFIX);

	assert(path_out && file_path);

	if (git_futils_readbuffer(&file, file_path) < 0)
		return -1;

	git_buf_rtrim(&file);
	/* apparently on Windows, some people use backslashes in paths */
	git_path_mkposix(file.ptr);

	if (git_buf_len(&file) <= prefix_len ||
		memcmp(git_buf_cstr(&file), GIT_GITLINK_FILE_PREFIX, prefix_len) != 0)
	{
		git_error_set(GIT_ERROR_REPOSITORY,
			"the `.git` file at '%s' is malformed", file_path);
		error = -1;
	}
	else if ((error = git_path_dirname_r(path_out, file_path)) >= 0) {
		const char *gitlink = git_buf_cstr(&file) + prefix_len;
		while (*gitlink && git__isspace(*gitlink)) gitlink++;

		error = git_path_prettify_dir(
			path_out, gitlink, git_buf_cstr(path_out));
	}

	git_buf_dispose(&file);
	return error;
}

int git_layout_write_template(
	const char *git_dir,
	bool allow_overwrite,
	const char *file,
	mode_t mode,
	bool hidden,
	const char *content)
{
	git_buf path = GIT_BUF_INIT;
	int fd, error = 0, flags;

	if (git_buf_joinpath(&path, git_dir, file) < 0)
		return -1;

	if (allow_overwrite)
		flags = O_WRONLY | O_CREAT | O_TRUNC;
	else
		flags = O_WRONLY | O_CREAT | O_EXCL;

	fd = p_open(git_buf_cstr(&path), flags, mode);

	if (fd >= 0) {
		error = p_write(fd, content, strlen(content));

		p_close(fd);
	}
	else if (errno != EEXIST)
		error = fd;

#ifdef GIT_WIN32
	if (!error && hidden) {
		if (git_win32__set_hidden(path.ptr, true) < 0)
			error = -1;
	}
#else
	GIT_UNUSED(hidden);
#endif

	git_buf_dispose(&path);

	if (error)
		git_error_set(GIT_ERROR_OS,
			"failed to initialize repository with template '%s'", file);

	return error;
}

int git_layout_write_gitlink(
	const char *in_dir, const char *to_repo, bool use_relative_path)
{
	int error;
	git_buf buf = GIT_BUF_INIT;
	git_buf path_to_repo = GIT_BUF_INIT;
	struct stat st;

	git_path_dirname_r(&buf, to_repo);
	git_path_to_dir(&buf);
	if (git_buf_oom(&buf))
		return -1;

	/* don't write gitlink to natural workdir */
	if (git__suffixcmp(to_repo, "/" DOT_GIT "/") == 0 &&
		strcmp(in_dir, buf.ptr) == 0)
	{
		error = GIT_PASSTHROUGH;
		goto cleanup;
	}

	if ((error = git_buf_joinpath(&buf, in_dir, DOT_GIT)) < 0)
		goto cleanup;

	if (!p_stat(buf.ptr, &st) && !S_ISREG(st.st_mode)) {
		git_error_set(GIT_ERROR_REPOSITORY,
			"cannot overwrite gitlink file into path '%s'", in_dir);
		error = GIT_EEXISTS;
		goto cleanup;
	}

	git_buf_clear(&buf);

	error = git_buf_sets(&path_to_repo, to_repo);

	if (!error && use_relative_path)
		error = git_path_make_relative(&path_to_repo, in_dir);

	if (!error)
		error = git_buf_join(&buf, ' ', GIT_GITLINK_FILE_PREFIX, path_to_repo.ptr);

	if (!error)
		error = git_layout_write_template(in_dir, true, DOT_GIT, 0666, true, buf.ptr);

cleanup:
	git_buf_dispose(&buf);
	git_buf_dispose(&path_to_repo);
	return error;
}
