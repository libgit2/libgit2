#include "common.h"

#define NON_EXISTING_HEAD "refs/heads/hide/and/seek"

extern void make_head_orphaned(git_repository* repo, const char *target);
