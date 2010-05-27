#ifndef INCLUDE_commit_h__
#define INCLUDE_commit_h__

#include "git/commit.h"
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

struct git_commit {
	git_revpool_object object;

	time_t commit_time;
	git_commit_list parents;

	unsigned short in_degree;
	unsigned parsed:1,
			 seen:1,
			 uninteresting:1,
			 topo_delay:1,
			 flags:26;
};

int git_commit__parse_oid(git_oid *oid, char **buffer_out, const char *buffer_end, const char *header);
int git_commit__parse_buffer(git_commit *commit, void *data, size_t len);
int git_commit__parse_time(time_t *commit_time, char *buffer, const char *buffer_end);
void git_commit__mark_uninteresting(git_commit *commit);

int git_commit_parse_existing(git_commit *commit);


int git_commit_list_push_back(git_commit_list *list, git_commit *commit);
int git_commit_list_push_front(git_commit_list *list, git_commit *commit);

git_commit *git_commit_list_pop_back(git_commit_list *list);
git_commit *git_commit_list_pop_front(git_commit_list *list);

void git_commit_list_clear(git_commit_list *list, int free_commits);

void git_commit_list_timesort(git_commit_list *list);
void git_commit_list_toposort(git_commit_list *list);

#endif
