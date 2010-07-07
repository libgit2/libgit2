#ifndef INCLUDE_commit_h__
#define INCLUDE_commit_h__

#include "git/commit.h"
#include "tree.h"
#include "revobject.h"

#include <time.h>

struct git_commit_node {
	struct git_commit *commit;

	struct git_commit_node *next;
	struct git_commit_node *prev;
};

struct git_commit_list {
	struct git_commit_node *head;
	struct git_commit_node *tail;
	size_t size;
};

typedef struct git_commit_list git_commit_list;
typedef struct git_commit_node git_commit_node;

#define GIT_COMMIT_TREE		(1 << 1)
#define GIT_COMMIT_PARENTS	(1 << 2)
#define GIT_COMMIT_AUTHOR	(1 << 3)
#define GIT_COMMIT_COMMITTER (1 << 4)
#define GIT_COMMIT_TIME		(1 << 5)
#define GIT_COMMIT_MESSAGE	(1 << 6)
#define GIT_COMMIT_MESSAGE_SHORT (1 << 7)
#define GIT_COMMIT_FOOTERS	(1 << 8)

struct git_commit {
	git_revpool_object object;
	git_obj odb_object;

	time_t commit_time;
	git_commit_list parents;

	git_tree *tree;
	git_commit_person *author;
	git_commit_person *committer;

	char *message;
	char *message_short;

	unsigned short in_degree;
	unsigned basic_parse:1,
			 odb_open:1,
			 seen:1,
			 uninteresting:1,
			 topo_delay:1,
			 flags:25;
};

void git_commit__free(git_commit *c);
int git_commit__parse(git_commit *commit, unsigned int flags, int close_odb);
int git_commit__parse_basic(git_commit *commit);
int git_commit__parse_oid(git_oid *oid, char **buffer_out, const char *buffer_end, const char *header);
int git_commit__parse_buffer(git_commit *commit, void *data, size_t len, unsigned int parse_flags);
int git_commit__parse_person(git_commit_person *person, char **buffer_out, const char *buffer_end, const char *header);
void git_commit__mark_uninteresting(git_commit *commit);


int git_commit_list_push_back(git_commit_list *list, git_commit *commit);
int git_commit_list_push_front(git_commit_list *list, git_commit *commit);

git_commit *git_commit_list_pop_back(git_commit_list *list);
git_commit *git_commit_list_pop_front(git_commit_list *list);

void git_commit_list_clear(git_commit_list *list, int free_commits);

void git_commit_list_timesort(git_commit_list *list);
void git_commit_list_toposort(git_commit_list *list);

#endif
