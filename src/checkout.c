/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include <assert.h>

#include "git2/checkout.h"
#include "git2/repository.h"
#include "git2/refs.h"
#include "git2/tree.h"
#include "git2/commit.h"

#include "common.h"
#include "refs.h"
#include "buffer.h"

GIT_BEGIN_DECL


static int get_head_tree(git_tree **out, git_repository *repo)
{
   int retcode = GIT_ERROR;
   git_reference *head = NULL;

   /* Dereference HEAD all the way to an OID ref */
   if (!git_reference_lookup_resolved(&head, repo, GIT_HEAD_FILE, -1)) {
      /* The OID should be a commit */
      git_object *commit;
      if (!git_object_lookup(&commit, repo,
                             git_reference_oid(head), GIT_OBJ_COMMIT)) {
         /* Get the tree */
         if (!git_commit_tree(out, (git_commit*)commit)) {
            retcode = 0;
         }
         git_object_free(commit);
      }
      git_reference_free(head);
   }

   return retcode;
}

typedef struct tree_walk_data
{
   git_indexer_stats *stats;
} tree_walk_data;


static int count_walker(const char *path, git_tree_entry *entry, void *payload)
{
   GIT_UNUSED(path);
   GIT_UNUSED(entry);
   ((tree_walk_data*)payload)->stats->total++;
   return 0;
}

static int checkout_walker(const char *path, git_tree_entry *entry, void *payload)
{
   int retcode = 0;
   tree_walk_data *data = (tree_walk_data*)payload;

   switch(git_tree_entry_type(entry)) {
   case GIT_OBJ_TREE:
      /* TODO: mkdir */
      break;

   case GIT_OBJ_BLOB:
      /* TODO: create/populate file */
      break;

   default:
      retcode = -1;
      break;
   }

   data->stats->processed++;
   return retcode;
}

/* TODO
 * -> Line endings
 */
int git_checkout_force(git_repository *repo, git_indexer_stats *stats)
{
   int retcode = GIT_ERROR;
   git_indexer_stats dummy_stats;
   git_tree *tree;
   tree_walk_data payload;

   assert(repo);
   if (!stats) stats = &dummy_stats;

   stats->total = stats->processed = 0;
   payload.stats = stats;

   if (!get_head_tree(&tree, repo)) {
      /* Count all the tree nodes for progress information */
      if (!git_tree_walk(tree, count_walker, GIT_TREEWALK_POST, &payload)) {
         /* Checkout the files */
         if (!git_tree_walk(tree, checkout_walker, GIT_TREEWALK_POST, &payload)) {
            retcode = 0;
         }
      }
      git_tree_free(tree);
   }

   return retcode;
}


GIT_END_DECL
