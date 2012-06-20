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


static int git_checkout_force(git_repository *repo)
{
   /* TODO */
   return 0;
}

static int update_head_to_remote(git_repository *repo, git_remote *remote)
{
   int retcode = 0;

   /* Get the remote's HEAD. This is always the first ref in remote->refs. */
   git_buf remote_default_branch = GIT_BUF_INIT;
   /* TODO */

   git_buf_free(&remote_default_branch);

   return retcode;
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

   /* Create the "origin" remote */
   if (!git_remote_add(&origin, repo, "origin", origin_url)) {
      /* Connect and download everything */
      if (!git_remote_connect(origin, GIT_DIR_FETCH)) {
         if (!git_remote_download(origin, &bytes, stats)) {
            /* Create "origin/foo" branches for all remote branches */
            if (!git_remote_update_tips(origin, NULL)) {
               /* Point HEAD to the same ref as the remote's head */
               if (!update_head_to_remote(repo, origin)) {
                  retcode = 0;
               }
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
                          const char *path,
                          git_indexer_stats *stats,
                          int is_bare)
{
   int retcode = GIT_ERROR;
   git_repository *repo = NULL;

   if (git_path_exists(path)) {
      giterr_set(GITERR_INVALID, "Path '%s' already exists.", path);
      return GIT_ERROR;
   }

   if (!(retcode = git_repository_init(&repo, path, is_bare))) {
      if ((retcode = setup_remotes_and_fetch(repo, origin_url, stats)) < 0) {
         /* Failed to fetch; clean up */
         git_repository_free(repo);
         git_futils_rmdir_r(path, GIT_DIRREMOVAL_FILES_AND_DIRS);
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
   return clone_internal(out, origin_url, dest_path, stats, 1);
}


int git_clone(git_repository **out,
              const char *origin_url,
              const char *workdir_path,
              git_indexer_stats *stats)
{
   int retcode = GIT_ERROR;

   if (!(retcode = clone_internal(out, origin_url, workdir_path, stats, 0))) {
      retcode = git_checkout_force(*out);
   }

   return retcode;
}




GIT_END_DECL
