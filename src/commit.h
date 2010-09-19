#ifndef INCLUDE_commit_h__
#define INCLUDE_commit_h__

#include "git/commit.h"
#include "tree.h"
#include "repository.h"

#include <time.h>

#define GIT_COMMIT_TREE		(1 << 1)
#define GIT_COMMIT_PARENTS	(1 << 2)
#define GIT_COMMIT_AUTHOR	(1 << 3)
#define GIT_COMMIT_COMMITTER (1 << 4)
#define GIT_COMMIT_TIME		(1 << 5)
#define GIT_COMMIT_MESSAGE	(1 << 6)
#define GIT_COMMIT_MESSAGE_SHORT (1 << 7)
#define GIT_COMMIT_FOOTERS	(1 << 8)

typedef struct git_commit_parents {
	git_commit *commit;
	struct git_commit_parents *next;
} git_commit_parents;

struct git_commit {
	git_object object;

	time_t commit_time;
	git_commit_parents *parents;

	git_tree *tree;
	git_person *author;
	git_person *committer;

	char *message;
	char *message_short;

	unsigned basic_parse:1,
			 odb_open:1;
};

void git_commit__free(git_commit *c);
int git_commit__parse(git_commit *commit, unsigned int flags, int close_odb);
int git_commit__parse_basic(git_commit *commit);
int git_commit__parse_buffer(git_commit *commit, void *data, size_t len, unsigned int parse_flags);
void git_commit__mark_uninteresting(git_commit *commit);

int git__parse_oid(git_oid *oid, char **buffer_out, const char *buffer_end, const char *header);
int git__parse_person(git_person *person, char **buffer_out, const char *buffer_end, const char *header);


#endif
