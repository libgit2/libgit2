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
	{ GIT_REPOSITORY_ITEM_GITDIR, GIT_REPOSITORY_ITEM__LAST, NULL, true },
	{ GIT_REPOSITORY_ITEM_WORKDIR, GIT_REPOSITORY_ITEM__LAST, NULL, true },
	{ GIT_REPOSITORY_ITEM_COMMONDIR, GIT_REPOSITORY_ITEM__LAST, NULL, true },
	{ GIT_REPOSITORY_ITEM_GITDIR, GIT_REPOSITORY_ITEM__LAST, "index", false },
	{ GIT_REPOSITORY_ITEM_COMMONDIR, GIT_REPOSITORY_ITEM_GITDIR, "objects", true },
	{ GIT_REPOSITORY_ITEM_COMMONDIR, GIT_REPOSITORY_ITEM_GITDIR, "refs", true },
	{ GIT_REPOSITORY_ITEM_COMMONDIR, GIT_REPOSITORY_ITEM_GITDIR, "packed-refs", false },
	{ GIT_REPOSITORY_ITEM_COMMONDIR, GIT_REPOSITORY_ITEM_GITDIR, "remotes", true },
	{ GIT_REPOSITORY_ITEM_COMMONDIR, GIT_REPOSITORY_ITEM_GITDIR, "config", false },
	{ GIT_REPOSITORY_ITEM_COMMONDIR, GIT_REPOSITORY_ITEM_GITDIR, "info", true },
	{ GIT_REPOSITORY_ITEM_COMMONDIR, GIT_REPOSITORY_ITEM_GITDIR, "hooks", true },
	{ GIT_REPOSITORY_ITEM_COMMONDIR, GIT_REPOSITORY_ITEM_GITDIR, "logs", true },
	{ GIT_REPOSITORY_ITEM_GITDIR, GIT_REPOSITORY_ITEM__LAST, "modules", true },
	{ GIT_REPOSITORY_ITEM_COMMONDIR, GIT_REPOSITORY_ITEM_GITDIR, "worktrees", true }
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
