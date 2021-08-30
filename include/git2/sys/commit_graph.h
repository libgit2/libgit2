/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_sys_git_commit_graph_h__
#define INCLUDE_sys_git_commit_graph_h__

#include "git2/common.h"
#include "git2/types.h"

/**
 * @file git2/sys/commit_graph.h
 * @brief Git commit-graph
 * @defgroup git_commit_graph Git commit-graph APIs
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * Opens a `git_commit_graph` from a path to an objects directory.
 *
 * This finds, opens, and validates the `commit-graph` file.
 *
 * @param cgraph_out the `git_commit_graph` struct to initialize.
 * @param objects_dir the path to a git objects directory.
 * @return Zero on success; -1 on failure.
 */
GIT_EXTERN(int) git_commit_graph_open(git_commit_graph **cgraph_out, const char *objects_dir);

/**
 * Frees commit-graph data. This should only be called when memory allocated
 * using `git_commit_graph_open` is not returned to libgit2 because it was not
 * associated with the ODB through a successful call to
 * `git_odb_set_commit_graph`.
 *
 * @param cgraph the commit-graph object to free. If NULL, no action is taken.
 */
GIT_EXTERN(void) git_commit_graph_free(git_commit_graph *cgraph);

GIT_END_DECL

#endif
