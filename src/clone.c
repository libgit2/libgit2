/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include <assert.h>

#include "git2/clone.h"
#include "git2/remote.h"
#include "git2/revparse.h"

#include "common.h"
#include "remote.h"
#include "fileops.h"
// TODO #include "checkout.h"

GIT_BEGIN_DECL


static int git_checkout_branch(git_repository *repo, const char *branchname)
{
  /* TODO */
  return 0;
}

/*
 * submodules?
 * filemodes?
 */



static int setup_remotes_and_fetch(git_repository *repo,
                                   const char *origin_url,
                                   git_indexer_stats *stats)
{
  int retcode = GIT_ERROR;
  git_remote *origin = NULL;
  git_off_t bytes = 0;
  git_indexer_stats dummy_stats;

  if (!stats) stats = &dummy_stats;

  if (!git_remote_add(&origin, repo, "origin", origin_url)) {
    if (!git_remote_connect(origin, GIT_DIR_FETCH)) {
      if (!git_remote_download(origin, &bytes, stats)) {
        if (!git_remote_update_tips(origin, NULL)) {
          retcode = 0;
        }
      }
      git_remote_disconnect(origin);
    }
    git_remote_free(origin);
  }

  return retcode;
}

static int clone_internal(git_repository **out,
                          const char *origin_url,
                          const char *fullpath,
                          git_indexer_stats *stats,
                          int is_bare)
{
  int retcode = GIT_ERROR;
  git_repository *repo = NULL;

  if (!(retcode = git_repository_init(&repo, fullpath, is_bare))) {
    if ((retcode = setup_remotes_and_fetch(repo, origin_url, stats)) < 0) {
      /* Failed to fetch; clean up */
      git_repository_free(repo);
      git_futils_rmdir_r(fullpath, GIT_DIRREMOVAL_FILES_AND_DIRS);
    } else {
      *out = repo;
      retcode = 0;
    }
  }

  return retcode;
}

int git_clone_bare(git_repository **out,
                   const char *origin_url,
                   const char *dest_path,
                   git_indexer_stats *stats)
{
  char fullpath[512] = {0};

  p_realpath(dest_path, fullpath);
  if (git_path_exists(fullpath)) {
    giterr_set(GITERR_INVALID, "Destination already exists: %s", fullpath);
    return GIT_ERROR;
  }

  return clone_internal(out, origin_url, fullpath, stats, 1);
}


int git_clone(git_repository **out,
              const char *origin_url,
              const char *workdir_path,
              git_indexer_stats *stats)
{
  int retcode = GIT_ERROR;
  char fullpath[512] = {0};

  p_realpath(workdir_path, fullpath);
  if (git_path_exists(fullpath)) {
    giterr_set(GITERR_INVALID, "Destination already exists: %s", fullpath);
    return GIT_ERROR;
  }

  if (!clone_internal(out, origin_url, workdir_path, stats, 0)) {
    char default_branch_name[256] = "master";
    /* TODO */
    retcode = git_checkout_branch(*out, default_branch_name);
  }

  return retcode;
}




GIT_END_DECL
