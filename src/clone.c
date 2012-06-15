/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include <assert.h>

#include "git2/clone.h"
#include "git2/remote.h"

#include "common.h"
#include "remote.h"
#include "fileops.h"
// TODO #include "checkout.h"

GIT_BEGIN_DECL

/*
 * submodules?
 * filemodes?
 */

static int setup_remotes_and_fetch(git_repository *repo, const char *origin_url)
{
  int retcode = GIT_ERROR;
  git_remote *origin = NULL;
  git_off_t bytes = 0;
  git_indexer_stats stats = {0};

  if (!git_remote_new(&origin, repo, "origin", origin_url, NULL)) {
    if (!git_remote_save(origin)) {
      if (!git_remote_connect(origin, GIT_DIR_FETCH)) {
        if (!git_remote_download(origin, &bytes, &stats)) {
          if (!git_remote_update_tips(origin, NULL)) {
            // TODO
            // if (!git_checkout(...)) {
            retcode = 0;
            // }
          }
        }
        git_remote_disconnect(origin);
      }
    }
    git_remote_free(origin);
 }

  return retcode;
}

int git_clone(git_repository **out, const char *origin_url, const char *dest_path)
{
  int retcode = GIT_ERROR;
  git_repository *repo = NULL;
  char fullpath[512] = {0};

  p_realpath(dest_path, fullpath);
  if (git_path_exists(fullpath)) {
    giterr_set(GITERR_INVALID, "Destination already exists: %s", fullpath);
    return GIT_ERROR;
  }

  /* Initialize the dest/.git directory */
  if (!(retcode = git_repository_init(&repo, fullpath, 0))) {
    if ((retcode = setup_remotes_and_fetch(repo, origin_url)) < 0) {
      /* Failed to fetch; clean up */
      git_repository_free(repo);
      git_futils_rmdir_r(fullpath, GIT_DIRREMOVAL_FILES_AND_DIRS);
    } else {
      /* Fetched successfully, do a checkout */
      /* if (!(retcode = git_checkout(...))) {} */
      *out = repo;
      retcode = 0;
    }
  }

  return retcode;
}


int git_clone_bare(git_repository **out, const char *origin_url, const char *dest_path)
{
  int retcode = GIT_ERROR;
  git_repository *repo = NULL;
  char fullpath[512] = {0};

  p_realpath(dest_path, fullpath);
  if (git_path_exists(fullpath)) {
    giterr_set(GITERR_INVALID, "Destination already exists: %s", fullpath);
    return GIT_ERROR;
  }

  if (!(retcode = git_repository_init(&repo, fullpath, 1))) {
    if ((retcode = setup_remotes_and_fetch(repo, origin_url)) < 0) {
      /* Failed to fetch; clean up */
      git_repository_free(repo);
      git_futils_rmdir_r(fullpath, GIT_DIRREMOVAL_FILES_AND_DIRS);
    } else {
      /* Fetched successfully, do a checkout */
      /* if (!(retcode = git_checkout(...))) {} */
      *out = repo;
      retcode = 0;
    }
  }

  return retcode;
}



GIT_END_DECL
