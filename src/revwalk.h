#ifndef INCLUDE_revwalk_h__
#define INCLUDE_revwalk_h__

#include "git/common.h"
#include "git/revwalk.h"

#include "commit.h"
#include "repository.h"
#include "hashtable.h"

struct git_revwalk_commit;

typedef struct git_revwalk_listnode {
	struct git_revwalk_commit *walk_commit;
	struct git_revwalk_listnode *next;
	struct git_revwalk_listnode *prev;
} git_revwalk_listnode;

typedef struct git_revwalk_list {
	struct git_revwalk_listnode *head;
	struct git_revwalk_listnode *tail;
	size_t size;
} git_revwalk_list;


struct git_revwalk_commit {

	git_commit *commit_object;
	git_revwalk_list parents;

	unsigned short in_degree;
	unsigned seen:1,
			 uninteresting:1,
			 topo_delay:1,
			 flags:25;
};

typedef struct git_revwalk_commit git_revwalk_commit;

struct git_revwalk {
	git_repository *repo;

	git_hashtable *commits;
	git_revwalk_list iterator;

	git_revwalk_commit *(*next)(git_revwalk_list *);

	unsigned walking:1;
	unsigned int sorting;
};


void git_revwalk__prepare_walk(git_revwalk *walk);
int git_revwalk__enroot(git_revwalk *walk, git_commit *commit);

int git_revwalk_list_push_back(git_revwalk_list *list, git_revwalk_commit *commit);
int git_revwalk_list_push_front(git_revwalk_list *list, git_revwalk_commit *obj);

git_revwalk_commit *git_revwalk_list_pop_back(git_revwalk_list *list);
git_revwalk_commit *git_revwalk_list_pop_front(git_revwalk_list *list);

void git_revwalk_list_clear(git_revwalk_list *list);

void git_revwalk_list_timesort(git_revwalk_list *list);
void git_revwalk_list_toposort(git_revwalk_list *list);

#endif /* INCLUDE_revwalk_h__ */
