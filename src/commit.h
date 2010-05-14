#ifndef INCLUDE_commit_h__
#define INCLUDE_commit_h__

#include "git/commit.h"

#include <time.h>

#define GIT_COMMIT_SEEN     (1 << 0)
#define GIT_COMMIT_HIDE     (1 << 1)
#define GIT_COMMIT_DELAY    (1 << 2)

struct git_commit {
    git_oid id;
    time_t commit_time;
    git_revpool *pool;
    unsigned parsed:1,
             flags:26;
};

int git_commit__parse_oid(git_oid *oid, char **buffer_out, const char *buffer_end, const char *header);
int git_commit__parse_buffer(git_commit *commit, void *data, size_t len);
int git_commit__parse_time(time_t *commit_time, char *buffer, const char *buffer_end);

#endif
