/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include <assert.h>

#include "common.h"
#include "buffer.h"

#include "git2/revparse.h"
#include "git2/object.h"
#include "git2/oid.h"
#include "git2/refs.h"
#include "git2/tag.h"
#include "git2/commit.h"
#include "git2/reflog.h"
#include "git2/refs.h"
#include "git2/repository.h"
#include "git2/config.h"

GIT_BEGIN_DECL

typedef enum {
   REVPARSE_STATE_INIT,
   REVPARSE_STATE_CARET,
   REVPARSE_STATE_LINEAR,
   REVPARSE_STATE_DONE,
} revparse_state;

static void set_invalid_syntax_err(const char *spec)
{
   giterr_set(GITERR_INVALID, "Refspec '%s' is not valid.", spec);
}

static int revparse_lookup_fully_qualifed_ref(git_object **out, git_repository *repo, const char*spec)
{
   git_reference *ref;
   git_object *obj = NULL;

   if (!git_reference_lookup(&ref, repo, spec)) {
      git_reference *resolved_ref;
      if (!git_reference_resolve(&resolved_ref, ref)) {
         if (!git_object_lookup(&obj, repo, git_reference_oid(resolved_ref), GIT_OBJ_ANY)) {
            *out = obj;
         }
         git_reference_free(resolved_ref);
      }
      git_reference_free(ref);
   }
   if (obj) {
      return 0;
   }

   return GIT_ERROR;
}

static int revparse_lookup_object(git_object **out, git_repository *repo, const char *spec)
{
   size_t speclen = strlen(spec);
   git_object *obj = NULL;
   git_oid oid;
   git_buf refnamebuf = GIT_BUF_INIT;
   static const char* formatters[] = {
      "refs/%s",
      "refs/tags/%s",
      "refs/heads/%s",
      "refs/remotes/%s",
      "refs/remotes/%s/HEAD",
      NULL
   };
   unsigned int i;
   const char *substr;

   /* "git describe" output; snip everything before/including "-g" */
   substr = strstr(spec, "-g");
   if (substr) {
      spec = substr + 2;
      speclen = strlen(spec);
   }

   /* SHA or prefix */
   if (!git_oid_fromstrn(&oid, spec, speclen)) {
      if (!git_object_lookup_prefix(&obj, repo, &oid, speclen, GIT_OBJ_ANY)) {
         *out = obj;
         return 0;
      }
   }

   /* Fully-named ref */
   if (!revparse_lookup_fully_qualifed_ref(&obj, repo, spec)) {
      *out = obj;
      return 0;
   }

   /* Partially-named ref; match in this order: */
   for (i=0; formatters[i]; i++) {
      git_buf_clear(&refnamebuf);
      if (git_buf_printf(&refnamebuf, formatters[i], spec) < 0) {
         return GIT_ERROR;
      }

      if (!revparse_lookup_fully_qualifed_ref(&obj, repo, git_buf_cstr(&refnamebuf))) {
         git_buf_free(&refnamebuf);
         *out = obj;
         return 0;
      }
   }
   git_buf_free(&refnamebuf);

   giterr_set(GITERR_REFERENCE, "Refspec '%s' not found.", spec);
   return GIT_ERROR;
}


static int walk_ref_history(git_object **out, git_repository *repo, const char *refspec, const char *reflogspec)
{
   git_reference *ref;
   git_reflog *reflog = NULL;
   int n, retcode = GIT_ERROR;
   size_t i, refloglen;
   const git_reflog_entry *entry;
   git_buf buf = GIT_BUF_INIT;
   size_t refspeclen = strlen(refspec);

   if (git__prefixcmp(reflogspec, "@{") != 0 ||
       git__suffixcmp(reflogspec, "}") != 0) {
      giterr_set(GITERR_INVALID, "Bad reflogspec '%s'", reflogspec);
      return GIT_ERROR;
   }

   /* "@{-N}" form means walk back N checkouts. That means the HEAD log. */
   if (refspeclen == 0 && !git__prefixcmp(reflogspec, "@{-")) {
      if (git__strtol32(&n, reflogspec+3, NULL, 0) < 0 ||
          n < 1) {
         giterr_set(GITERR_INVALID, "Invalid reflogspec %s", reflogspec);
         return GIT_ERROR;
      }
      git_reference_lookup(&ref, repo, "HEAD");
      git_reflog_read(&reflog, ref);
      git_reference_free(ref);

      refloglen = git_reflog_entrycount(reflog);
      for (i=0; i < refloglen; i++) {
         const char *msg;
         entry = git_reflog_entry_byindex(reflog, i);

         msg = git_reflog_entry_msg(entry);
         if (!git__prefixcmp(msg, "checkout: moving")) {
            n--;
            if (!n) {
               char *branchname = strrchr(msg, ' ') + 1;
               git_buf_printf(&buf, "refs/heads/%s", branchname);
               retcode = revparse_lookup_fully_qualifed_ref(out, repo, git_buf_cstr(&buf));
               break;
            }
         }
      }
   } else {
      if (!refspeclen) {
         /* Empty refspec means current branch */
         /* Get the target of HEAD */
         git_reference_lookup(&ref, repo, "HEAD");
         git_buf_puts(&buf, git_reference_target(ref));
         git_reference_free(ref);
      } else {
         if (git__prefixcmp(refspec, "refs/heads/") != 0) {
            git_buf_printf(&buf, "refs/heads/%s", refspec);
         } else {
            git_buf_puts(&buf, refspec);
         }
      }

      /* @{u} or @{upstream} -> upstream branch, for a tracking branch. This is stored in the config. */
      if (!strcmp(reflogspec, "@{u}") || !strcmp(reflogspec, "@{upstream}")) {
         git_config *cfg;
         if (!git_repository_config(&cfg, repo)) {
            /* Is the ref a tracking branch? */
            const char *remote;
            git_buf_clear(&buf);
            git_buf_printf(&buf, "branch.%s.remote", refspec);
            if (!git_config_get_string(cfg, git_buf_cstr(&buf), &remote)) {
               /* Yes. Find the first merge target name. */
               const char *mergetarget;
               git_buf_clear(&buf);
               git_buf_printf(&buf, "branch.%s.merge", refspec);
               if (!git_config_get_string(cfg, git_buf_cstr(&buf), &mergetarget) &&
                   !git__prefixcmp(mergetarget, "refs/heads/")) {
                  /* Success. Look up the target and fetch the object. */
                  git_buf_clear(&buf);
                  git_buf_printf(&buf, "refs/remotes/%s/%s", remote, mergetarget+11);
                  retcode = revparse_lookup_fully_qualifed_ref(out, repo, git_buf_cstr(&buf));
               }
            }
            git_config_free(cfg);
         }
      }

      /* @{N} -> Nth prior value for the ref (from reflog) */
      else if (!git__strtol32(&n, reflogspec+2, NULL, 0)) {
         if (n == 0) {
            retcode = revparse_lookup_fully_qualifed_ref(out, repo, git_buf_cstr(&buf));
         } else if (!git_reference_lookup(&ref, repo, git_buf_cstr(&buf))) {
            if (!git_reflog_read(&reflog, ref)) {
               const git_reflog_entry *entry = git_reflog_entry_byindex(reflog, n);
               const git_oid *oid = git_reflog_entry_oidold(entry);
               retcode = git_object_lookup(out, repo, oid, GIT_OBJ_ANY);
            }
            git_reference_free(ref);
         }
      }

      /* @{Anything else} -> try to parse the expression into a date, and get the value of the ref as it
         was then. */
      else {
         /* TODO */
      }
   }

   if (reflog) git_reflog_free(reflog);
   git_buf_free(&buf);
   return retcode;
}

static git_object* dereference_object(git_object *obj)
{
   git_otype type = git_object_type(obj);

   switch (type) {
   case GIT_OBJ_COMMIT:
      {
         git_tree *tree = NULL;
         if (0 == git_commit_tree(&tree, (git_commit*)obj)) {
            return (git_object*)tree;
         }
      }
      break;
   case GIT_OBJ_TAG:
      {
            git_object *newobj = NULL;
            if (0 == git_tag_target(&newobj, (git_tag*)obj)) {
               return newobj;
            }
      }
      break;

   default:
   case GIT_OBJ_TREE:
   case GIT_OBJ_BLOB:
   case GIT_OBJ_OFS_DELTA:
   case GIT_OBJ_REF_DELTA:
      break;
   }

   /* Can't dereference some types */
   return NULL;
}

static int dereference_to_type(git_object **out, git_object *obj, git_otype target_type)
{
   int retcode = 1;
   git_object *obj1 = obj, *obj2 = obj;

   while (retcode > 0) {
      git_otype this_type = git_object_type(obj1);

      if (this_type == target_type) {
         *out = obj1;
         retcode = 0;
      } else {
         /* Dereference once, if possible. */
         obj2 = dereference_object(obj1);
         if (!obj2) {
            giterr_set(GITERR_REFERENCE, "Can't dereference to type");
            retcode = GIT_ERROR;
         }
      }
      if (obj1 != obj) {
         git_object_free(obj1);
      }
      obj1 = obj2;
   }
   return retcode;
}

static git_otype parse_obj_type(const char *str)
{
   if (!strcmp(str, "{commit}")) return GIT_OBJ_COMMIT;
   if (!strcmp(str, "{tree}")) return GIT_OBJ_TREE;
   if (!strcmp(str, "{blob}")) return GIT_OBJ_BLOB;
   if (!strcmp(str, "{tag}")) return GIT_OBJ_TAG;
   return GIT_OBJ_BAD;
}

static int handle_caret_syntax(git_object **out, git_object *obj, const char *movement)
{
   git_commit *commit;
   size_t movementlen = strlen(movement);
   int n;

   if (*movement == '{') {
      if (movement[movementlen-1] != '}') {
         set_invalid_syntax_err(movement);
         return GIT_ERROR;
      }

      /* {} -> Dereference until we reach an object that isn't a tag. */
      if (movementlen == 2) {
         git_object *newobj = obj;
         git_object *newobj2 = newobj;
         while (git_object_type(newobj2) == GIT_OBJ_TAG) {
            newobj2 = dereference_object(newobj);
            if (newobj != obj) git_object_free(newobj);
            if (!newobj2) {
               giterr_set(GITERR_REFERENCE, "Couldn't find object of target type.");
               return GIT_ERROR;
            }
            newobj = newobj2;
         }
         *out = newobj2;
         return 0;
      }
      
      /* {/...} -> Walk all commits until we see a commit msg that matches the phrase. */
      if (movement[1] == '/') {
         /* TODO */
         return GIT_ERROR;
      }

      /* {...} -> Dereference until we reach an object of a certain type. */
      if (dereference_to_type(out, obj, parse_obj_type(movement)) < 0) {
         return GIT_ERROR;
      }
      return 0;
   }

   /* Dereference until we reach a commit. */
   if (dereference_to_type(&obj, obj, GIT_OBJ_COMMIT) < 0) {
      /* Can't dereference to a commit; fail */
      return GIT_ERROR;
   }

   /* "^" is the same as "^1" */
   if (movementlen == 0) {
      n = 1;
   } else {
      git__strtol32(&n, movement, NULL, 0);
   }
   commit = (git_commit*)obj;

   /* "^0" just returns the input */
   if (n == 0) {
      *out = obj;
      return 0;
   }

   if (git_commit_parent(&commit, commit, n-1) < 0) {
      return GIT_ERROR;
   }

   *out = (git_object*)commit;
   return 0;
}

static int handle_linear_syntax(git_object **out, git_object *obj, const char *movement)
{
   git_commit *commit1, *commit2;
   int i, n;

   /* Dereference until we reach a commit. */
   if (dereference_to_type(&obj, obj, GIT_OBJ_COMMIT) < 0) {
      /* Can't dereference to a commit; fail */
      return GIT_ERROR;
   }

   /* "~" is the same as "~1" */
   if (strlen(movement) == 0) {
      n = 1;
   } else {
      git__strtol32(&n, movement, NULL, 0);
   }
   commit1 = (git_commit*)obj;

   /* "~0" just returns the input */
   if (n == 0) {
      *out = obj;
      return 0;
   }

   for (i=0; i<n; i++) {
      if (git_commit_parent(&commit2, commit1, 0) < 0) {
         return GIT_ERROR;
      }
      if (commit1 != (git_commit*)obj) {
         git_commit_free(commit1);
      }
      commit1 = commit2;
   }

   *out = (git_object*)commit1;
   return 0;
}

int git_revparse_single(git_object **out, git_repository *repo, const char *spec)
{
   revparse_state current_state = REVPARSE_STATE_INIT,  next_state = REVPARSE_STATE_INIT;
   const char *spec_cur = spec;
   git_object *cur_obj = NULL,  *next_obj = NULL;
   git_buf specbuffer = GIT_BUF_INIT,  stepbuffer = GIT_BUF_INIT;
   int retcode = 0;

   assert(out && repo && spec);

   while (current_state != REVPARSE_STATE_DONE) {
      switch (current_state) {
      case REVPARSE_STATE_INIT:
         if (!*spec_cur) {
            /* No operators, just a name. Find it and return. */
            retcode = revparse_lookup_object(out, repo, spec);
            next_state = REVPARSE_STATE_DONE;
         } else if (*spec_cur == '@') {
            /* '@' syntax doesn't allow chaining */
            git_buf_puts(&stepbuffer, spec_cur);
            retcode = walk_ref_history(out, repo, git_buf_cstr(&specbuffer), git_buf_cstr(&stepbuffer));
            next_state = REVPARSE_STATE_DONE;
         } else if (*spec_cur == '^') {
            next_state = REVPARSE_STATE_CARET;
         } else if (*spec_cur == '~') {
            next_state = REVPARSE_STATE_LINEAR;
         } else {
            git_buf_putc(&specbuffer, *spec_cur);
         }
         spec_cur++;

         if (current_state != next_state && next_state != REVPARSE_STATE_DONE) {
            /* Leaving INIT state, find the object specified, in case that state needs it */
            revparse_lookup_object(&next_obj, repo, git_buf_cstr(&specbuffer));
         }
         break;


      case REVPARSE_STATE_CARET:
         /* Gather characters until NULL, '~', or '^' */
         if (!*spec_cur) {
            retcode = handle_caret_syntax(out, cur_obj, git_buf_cstr(&stepbuffer));
            next_state = REVPARSE_STATE_DONE;
         } else if (*spec_cur == '~') {
            retcode = handle_caret_syntax(&next_obj, cur_obj, git_buf_cstr(&stepbuffer));
            git_buf_clear(&stepbuffer);
            next_state = !retcode ? REVPARSE_STATE_LINEAR : REVPARSE_STATE_DONE;
         } else if (*spec_cur == '^') {
            retcode = handle_caret_syntax(&next_obj, cur_obj, git_buf_cstr(&stepbuffer));
            git_buf_clear(&stepbuffer);
            if (retcode < 0) {
               next_state = REVPARSE_STATE_DONE;
            }
         } else {
            git_buf_putc(&stepbuffer, *spec_cur);
         }
         spec_cur++;
         break;

      case REVPARSE_STATE_LINEAR:
         if (!*spec_cur) {
            retcode = handle_linear_syntax(out, cur_obj, git_buf_cstr(&stepbuffer));
            next_state = REVPARSE_STATE_DONE;
         } else if (*spec_cur == '~') {
            retcode = handle_linear_syntax(&next_obj, cur_obj, git_buf_cstr(&stepbuffer));
            git_buf_clear(&stepbuffer);
            if (retcode < 0) {
               next_state = REVPARSE_STATE_DONE;
            }
         } else if (*spec_cur == '^') {
            retcode = handle_linear_syntax(&next_obj, cur_obj, git_buf_cstr(&stepbuffer));
            git_buf_clear(&stepbuffer);
            next_state = !retcode ? REVPARSE_STATE_CARET : REVPARSE_STATE_DONE;
         } else {
            git_buf_putc(&stepbuffer, *spec_cur);
         }
         spec_cur++;
         break;

      case REVPARSE_STATE_DONE:
         if (cur_obj && *out != cur_obj) git_object_free(cur_obj);
         if (next_obj && *out != next_obj) git_object_free(next_obj);
         break;
      }

      current_state = next_state;
      if (cur_obj != next_obj) {
         if (cur_obj) git_object_free(cur_obj);
         cur_obj = next_obj;
      }
   }

   if (*out != cur_obj) git_object_free(cur_obj);
   if (*out != next_obj && next_obj != cur_obj) git_object_free(next_obj);

   git_buf_free(&specbuffer);
   git_buf_free(&stepbuffer);
   return retcode;
}


GIT_END_DECL
