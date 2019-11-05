/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_layout_h__
#define INCLUDE_layout_h__

#include "common.h"

#include "git2/layout.h"

#define DOT_GIT ".git"
#define GIT_DIR DOT_GIT "/"
#define GIT_DIR_MODE 0755
#define GIT_BARE_DIR_MODE 0777

#define GIT_COMMONDIR_FILE "commondir"
#define GIT_GITDIR_FILE "gitdir"

#define GIT_REFS_DIR "refs/"
#define GIT_REFS_HEADS_DIR GIT_REFS_DIR "heads/"
#define GIT_REFS_TAGS_DIR GIT_REFS_DIR "tags/"
#define GIT_REFS_REMOTES_DIR GIT_REFS_DIR "remotes/"
#define GIT_REFS_NOTES_DIR GIT_REFS_DIR "notes/"
#define GIT_REFS_DIR_MODE 0777
#define GIT_REFS_FILE_MODE 0666

#define GIT_PACKEDREFS_FILE "packed-refs"
#define GIT_PACKEDREFS_FILE_MODE 0666

#define GIT_HEAD_FILE "HEAD"
#define GIT_ORIG_HEAD_FILE "ORIG_HEAD"
#define GIT_FETCH_HEAD_FILE "FETCH_HEAD"
#define GIT_MERGE_HEAD_FILE "MERGE_HEAD"
#define GIT_REVERT_HEAD_FILE "REVERT_HEAD"
#define GIT_CHERRYPICK_HEAD_FILE "CHERRY_PICK_HEAD"
#define GIT_CHERRYPICK_FILE_MODE 0666
#define GIT_BISECT_LOG_FILE "BISECT_LOG"
#define GIT_REBASE_MERGE_DIR "rebase-merge/"
#define GIT_REBASE_MERGE_INTERACTIVE_FILE GIT_REBASE_MERGE_DIR "interactive"
#define GIT_REBASE_APPLY_DIR "rebase-apply/"
#define GIT_REBASE_APPLY_REBASING_FILE GIT_REBASE_APPLY_DIR "rebasing"
#define GIT_REBASE_APPLY_APPLYING_FILE GIT_REBASE_APPLY_DIR "applying"
#define GIT_REFS_HEADS_MASTER_FILE GIT_REFS_HEADS_DIR "master"

#define GIT_SEQUENCER_DIR "sequencer/"
#define GIT_SEQUENCER_HEAD_FILE GIT_SEQUENCER_DIR "head"
#define GIT_SEQUENCER_OPTIONS_FILE GIT_SEQUENCER_DIR "options"
#define GIT_SEQUENCER_TODO_FILE GIT_SEQUENCER_DIR "todo"

#define GIT_STASH_FILE "stash"
#define GIT_REFS_STASH_FILE GIT_REFS_DIR GIT_STASH_FILE

#define GIT_OBJECTS_DIR "objects/"
#define GIT_OBJECT_DIR_MODE 0777
#define GIT_OBJECT_FILE_MODE 0444

#define GIT_MERGE_MSG_FILE "MERGE_MSG"
#define GIT_MERGE_MODE_FILE "MERGE_MODE"
#define GIT_MERGE_FILE_MODE 0666

/** Internal structure for repository layout information */
typedef struct {
	char *gitdir; /**< Absolute path to the .git directory */
	char *workdir; /**< Absolute path to the working directory */
	char *commondir; /**< Absolute path to the common repository */
} git_repository_layout;

int git_layout_item_path(git_buf *out, const git_repository_layout *repo, git_repository_item_t item);

#endif
