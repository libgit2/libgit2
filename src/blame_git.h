
#ifndef INCLUDE_blame_git__
#define INCLUDE_blame_git__

#include "git2.h"
#include "blame.h"
#include "xdiff/xinclude.h"

int get_origin(git_blame__origin **out, git_blame *sb, git_commit *commit, const char *path);
int make_origin(git_blame__origin **out, git_commit *commit, const char *path);
git_blame__origin *origin_incref(git_blame__origin *o);
void origin_decref(git_blame__origin *o);
void assign_blame(git_blame *sb, uint32_t flags);
void coalesce(git_blame *sb);

#endif
