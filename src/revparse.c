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

GIT_BEGIN_DECL

typedef enum {
   REVPARSE_STATE_INIT,
   
   /* for parsing "@{...}" */
   REVPARSE_STATE_REF_A,
   REVPARSE_STATE_REF_B,

   /* for "^{...}" and ^... */
   REVPARSE_STATE_PARENTS_A,
   REVPARSE_STATE_PARENTS_B,

   /* For "~..." */
   REVPARSE_STATE_LIINEAR,

   /* For joining parents and linear, as in "master^2~3^2" */
   REVPARSE_STATE_JOIN,
} revparse_state;

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


static void set_invalid_syntax_err(const char *spec)
{
   giterr_set(GITERR_INVALID, "Refspec '%s' is not valid.", spec);
}


int git_revparse_single(git_object **out, git_repository *repo, const char *spec)
{
   revparse_state current_state = REVPARSE_STATE_INIT;
   revparse_state next_state = REVPARSE_STATE_INIT;
   const char *spec_cur = spec;
   git_object *obj = NULL;
   git_buf specbuffer = GIT_BUF_INIT;
   git_buf stepbuffer = GIT_BUF_INIT;

   assert(out && repo && spec);

   while (1) {
      switch (current_state) {
      case REVPARSE_STATE_INIT:
         if (!*spec_cur) {
            /* No operators, just a name. Find it and return. */
            return revparse_lookup_object(out, repo, spec);
         } else if (*spec_cur == '@') {
            next_state = REVPARSE_STATE_REF_A;
         }
         spec_cur++;

         if (current_state != next_state) {
            /* Leaving INIT state, find the object specified and carry on */
            assert(!git_buf_set(&specbuffer, spec, spec_cur - spec));
            assert(!revparse_lookup_object(&obj, repo, git_buf_cstr(&specbuffer)));
         }
         break;

      case REVPARSE_STATE_REF_A:
         /* Found '@', look for '{', fail otherwise */
         if (*spec_cur != '{') {
            set_invalid_syntax_err(spec);
            return GIT_ERROR;
         }
         spec_cur++;
         next_state = REVPARSE_STATE_REF_B;
         break;

      case REVPARSE_STATE_REF_B:
         /* Found "@{", gather things until a '}' */
         break;
      }

      current_state = next_state;
   }
   
   return 0;
}


GIT_END_DECL
