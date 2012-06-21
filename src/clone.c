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
#include "git2/branch.h"
#include "git2/config.h"

#include "common.h"
#include "remote.h"
#include "fileops.h"
#include "refs.h"
// TODO #include "checkout.h"

GIT_BEGIN_DECL

struct HeadInfo {
   git_repository *repo;
   git_oid remote_head_oid;
   git_buf branchname;
};

static int git_checkout_force(git_repository *repo)
{
   /* TODO
    * -> Line endings
    */
   return 0;
}

static int create_tracking_branch(struct HeadInfo *info)
{
   git_object *head_obj = NULL;
   git_oid branch_oid;
   int retcode = GIT_ERROR;
   const char *branchname = git_buf_cstr(&info->branchname);

   /* Find the target commit */
   if (git_object_lookup(&head_obj, info->repo, &info->remote_head_oid, GIT_OBJ_ANY) < 0)
      return GIT_ERROR;

   /* Create the new branch */
   if (!git_branch_create(&branch_oid, info->repo, branchname, head_obj, 0)) {
      /* Set up tracking */
      git_config *cfg;
      if (!git_repository_config(&cfg, info->repo)) {
         git_buf remote = GIT_BUF_INIT;
         git_buf merge = GIT_BUF_INIT;
         git_buf merge_target = GIT_BUF_INIT;
         if (!git_buf_printf(&remote, "branch.%s.remote", branchname) &&
             !git_buf_printf(&merge, "branch.%s.merge", branchname) &&
             !git_buf_printf(&merge_target, "refs/heads/%s", branchname) &&
             !git_config_set_string(cfg, git_buf_cstr(&remote), "origin") &&
             !git_config_set_string(cfg, git_buf_cstr(&merge), git_buf_cstr(&merge_target))) {
            retcode = 0;
         }
         git_buf_free(&remote);
         git_buf_free(&merge);
         git_buf_free(&merge_target);
         git_config_free(cfg);
      }
   }

   return retcode;
}

static int reference_matches_remote_head(const char *head_name, void *payload)
{
   struct HeadInfo *head_info = (struct HeadInfo *)payload;
   git_oid oid;

   /* Stop looking if we've already found a match */
   if (git_buf_len(&head_info->branchname) > 0) return 0;

   if (!git_reference_name_to_oid(&oid, head_info->repo, head_name) &&
       !git_oid_cmp(&head_info->remote_head_oid, &oid)) {
      /* strlen("refs/remotes/origin/") == 20 */
      git_buf_puts(&head_info->branchname, head_name+20);
   }
   return 0;
}

static int update_head_to_remote(git_repository *repo, git_remote *remote)
{
   int retcode = 0;
   git_remote_head *remote_head;
   struct HeadInfo head_info;

   /* Get the remote's HEAD. This is always the first ref in remote->refs. */
   remote_head = remote->refs.contents[0];
   git_oid_cpy(&head_info.remote_head_oid, &remote_head->oid);
   git_buf_init(&head_info.branchname, 16);
   head_info.repo = repo;

   /* Find the branch the remote head belongs to. */
   if (!git_reference_foreach(repo, GIT_REF_LISTALL, reference_matches_remote_head, &head_info) &&
       git_buf_len(&head_info.branchname) > 0) {
      if (!create_tracking_branch(&head_info)) {
         /* Update HEAD to point to the new branch */
         git_reference *head;
         if (!git_reference_lookup(&head, repo, "HEAD")) {
            git_buf target = GIT_BUF_INIT;
            if (!git_buf_printf(&target, "refs/heads/%s", git_buf_cstr(&head_info.branchname)) &&
                !git_reference_set_target(head, git_buf_cstr(&target))) {
               retcode = 0;
            }
            git_buf_free(&target);
            git_reference_free(head);
         }
      }
   }

   git_buf_free(&head_info.branchname);
   return retcode;
}

/*
 * submodules?
 * filemodes?
 * Line endings
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
