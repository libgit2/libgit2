#ifndef INCLUDE_index_h__
#define INCLUDE_index_h__

#include "fileops.h"
#include "filelock.h"
#include "git/odb.h"
#include "git/index.h"

#define GIT_IDXENTRY_NAMEMASK  (0x0fff)
#define GIT_IDXENTRY_STAGEMASK (0x3000)
#define GIT_IDXENTRY_EXTENDED  (0x4000)
#define GIT_IDXENTRY_VALID     (0x8000)
#define GIT_IDXENTRY_STAGESHIFT 12

struct git_index_tree {
	char *name;

	struct git_index_tree *parent;
	struct git_index_tree **children;
	size_t children_count;

	size_t entries;
	git_oid oid;
};

typedef struct git_index_tree git_index_tree;

struct git_index {
	git_repository *repository;
	char *index_file_path;

	time_t last_modified;

	git_index_entry *entries;
	unsigned int entries_size;

	unsigned int entry_count;
	unsigned int sorted:1,
				 on_disk:1;

	git_index_tree *tree;
};

int git_index__write(git_index *index, git_filelock *file);
void git_index__sort(git_index *index);
int git_index__parse(git_index *index, const char *buffer, size_t buffer_size);
int git_index__remove_pos(git_index *index, unsigned int position);
int git_index__append(git_index *index, const git_index_entry *entry);

void git_index_tree__free(git_index_tree *tree);

#endif
