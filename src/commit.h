#ifndef INCLUDE_commit_h__
#define INCLUDE_commit_h__

#include "git/commit.h"
#include "tree.h"
#include "repository.h"

#include <time.h>

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

	unsigned full_parse:1;
};

void git_commit__free(git_commit *c);
int git_commit__parse(git_commit *commit);
int git_commit__parse_full(git_commit *commit);
int git_commit__parse_buffer(git_commit *commit, void *data, size_t len, unsigned int parse_flags);

int git_commit__writeback(git_commit *commit, git_odb_source *src);

int git__parse_oid(git_oid *oid, char **buffer_out, const char *buffer_end, const char *header);
int git__parse_person(git_person *person, char **buffer_out, const char *buffer_end, const char *header);

int git__write_oid(git_odb_source *src, const char *header, const git_oid *oid);
int git__write_person(git_odb_source *src, const char *header, const git_person *person);

#endif
